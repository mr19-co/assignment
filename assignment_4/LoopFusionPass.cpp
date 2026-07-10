#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include <unordered_map>

using namespace llvm;

//-----------------------------------------------------------------------------
// LoopFusionPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {
  // New PM implementation
  struct LoopFusionPass: PassInfoMixin<LoopFusionPass> {
    bool are_loops_adjacent(Loop* l1, Loop* l2) {
      BasicBlock* l1_exit_block = l1->getExitBlock();
      BasicBlock* l2_preheader = l2->getLoopPreheader();

      if (!l1_exit_block || !l2_preheader) {
        return false;
      }

      return l1_exit_block == l2_preheader;
    }

    bool same_number_of_iterations(Loop* l1, Loop* l2, ScalarEvolution& SCE) {
      const SCEV* backedge_taken_count_l1 = SCE.getBackedgeTakenCount(l1);
      const SCEV* backedge_taken_count_l2 = SCE.getBackedgeTakenCount(l2);

      if (
        isa<SCEVCouldNotCompute>(backedge_taken_count_l1) ||
        isa<SCEVCouldNotCompute>(backedge_taken_count_l2)
      ) {      
        return false;
      }

      return backedge_taken_count_l1 == backedge_taken_count_l2;
    }

    bool are_loops_control_flow_equivalent(
      Loop *l1, Loop *l2, DominatorTree &DT, PostDominatorTree &PDT
    ) {
      BasicBlock* l1_header = l1->getHeader();
      BasicBlock* l2_header = l2->getHeader();

      return DT.dominates(l1_header, l2_header) && PDT.dominates(l2_header, l1_header);
    }

    bool no_neg_dist_dep(Loop *l1, Loop *l2, ScalarEvolution &SE) {
      SmallVector<Instruction*, 16> l1_mem_insts, l2_mem_insts;

      for (BasicBlock* basic_block : l1->blocks())
        for (Instruction& inst : *basic_block)
          if (isa<LoadInst>(inst) || isa<StoreInst>(inst))
            l1_mem_insts.push_back(&inst);

      for (BasicBlock* basic_block : l2->blocks())
        for (Instruction& inst : *basic_block)
          if (isa<LoadInst>(inst) || isa<StoreInst>(inst))
            l2_mem_insts.push_back(&inst);

      for (Instruction* l1_mem_inst : l1_mem_insts) {
        for (Instruction* l2_mem_inst : l2_mem_insts) {
          if (isa<LoadInst>(l1_mem_inst) && isa<LoadInst>(l2_mem_inst)) {
            continue;
          }

          Value* l1_mem_inst_ptr = getLoadStorePointerOperand(l1_mem_inst);
          Value* l2_mem_inst_ptr = getLoadStorePointerOperand(l2_mem_inst);
          if (!l1_mem_inst_ptr || !l2_mem_inst_ptr)
            return false;

          GetElementPtrInst* l1_gep = dyn_cast<GetElementPtrInst>(l1_mem_inst_ptr);
          Value* l1_base_ptr = l1_gep ? l1_gep->getPointerOperand(): l1_mem_inst_ptr;
          GetElementPtrInst* l2_gep = dyn_cast<GetElementPtrInst>(l2_mem_inst_ptr);
          Value* l2_base_ptr = l2_gep ? l2_gep->getPointerOperand(): l2_mem_inst_ptr;

          if (l1_base_ptr != l2_base_ptr)
            continue;

          const SCEV* l1_ptr_scev = SE.getSCEVAtScope(l1_mem_inst_ptr, l1);
          const SCEV* l2_ptr_scev = SE.getSCEVAtScope(l2_mem_inst_ptr, l2);

          const SCEV* diff;
          if (isa<LoadInst>(l1_mem_inst) && isa<StoreInst>(l2_mem_inst)) {
            diff = SE.getMinusSCEV(l2_ptr_scev, l1_ptr_scev);
          } else {
            diff = SE.getMinusSCEV(l1_ptr_scev, l2_ptr_scev);
          }

          if (isa<SCEVCouldNotCompute>(diff)) {
            return false;
          }

          const SCEV* diff_start_scev = diff;
          int diff_step = 0;
          while (const SCEVAddRecExpr* add_rec = dyn_cast<SCEVAddRecExpr>(diff_start_scev)) {
            diff_start_scev = add_rec->getStart();
            if (add_rec->getNumOperands() != 2) {
              return false;
            }
            const SCEVConstant* step_scev = dyn_cast<SCEVConstant>(add_rec->getOperand(1));
            if (!step_scev) { return false; }
            diff_step += step_scev->getValue()->getSExtValue();
          }

          if (diff_step != 0) {
            return false;
          }

          const SCEVConstant* diff_start_scev_const = dyn_cast<SCEVConstant>(diff_start_scev);
          if (!diff_start_scev_const) { return false; }
          int diff_start = diff_start_scev_const->getValue()->getSExtValue();

          if (diff_start < 0) {
            return false;
          }
        }
      }

      for (BasicBlock* l2_basic_block : l2->blocks()) {
        for (Instruction& l2_inst : *l2_basic_block) {
          for (Value* op : l2_inst.operands()) {
            if (Instruction* def = dyn_cast<Instruction>(op)) {
              if (l1->contains(def) && !l1->isLoopInvariant(def)) {
                return false;
              }
            }
          }
        }
      }

      return true;
    }

    BasicBlock* get_body_head(Loop *loop) {
      BranchInst *header_branch_inst = dyn_cast<BranchInst>(loop->getHeader()->getTerminator());
      if (!header_branch_inst || !header_branch_inst->isConditional()) {
        return nullptr;
      }
      for (int i = 0; i < header_branch_inst->getNumSuccessors(); ++i) {
        if (loop->contains(header_branch_inst->getSuccessor(i))) {
          return header_branch_inst->getSuccessor(i);
        }
      }
      return nullptr;
    }

    BasicBlock* get_body_tail(Loop* loop) {
      BasicBlock* header = loop->getHeader();
      BasicBlock* latch = loop->getLoopLatch();
      if (!latch) { return nullptr; }

      for (BasicBlock *basic_block : loop->blocks()) {
        if (basic_block == header) {
          continue;
        }
        BranchInst* branch_inst = dyn_cast<BranchInst>(basic_block->getTerminator());
        if (branch_inst && branch_inst->isUnconditional() && branch_inst->getSuccessor(0) == latch) {
          return basic_block;
        }
      }
      return nullptr;
    }

    PHINode* get_header_induction_variable(Loop* loop) {
      BasicBlock* header = loop->getHeader();
      BasicBlock* latch = loop->getLoopLatch();
      if (!latch) { return nullptr; }

      for (PHINode &phi : header->phis()) {
        if (phi.getBasicBlockIndex(latch) != -1) {
          return &phi;
        }
      }
      return nullptr;
    }

    bool fuse_loops(Loop* l1, Loop* l2, LoopInfo &LI) {

      BasicBlock* l1_header = l1->getHeader();
      BasicBlock* l1_latch = l1->getLoopLatch();
      BasicBlock* l1_exit_block = l1->getExitBlock();      

      BasicBlock* l2_preheader = l2->getLoopPreheader();
      BasicBlock* l2_header = l2->getHeader();
      BasicBlock* l2_latch = l2->getLoopLatch();
      BasicBlock* l2_exit_block = l2->getExitBlock();

      if (!l1_latch || !l2_latch || !l1_exit_block || !l2_exit_block) {
        return false;
      }

      BasicBlock *l1_body_head = get_body_head(l1);
      BasicBlock *l2_body_head = get_body_head(l2);
      if (!l1_body_head || !l2_body_head) {
        return false;
      }

      PHINode* l1_header_ind_var = get_header_induction_variable(l1);
      PHINode* l2_header_ind_var = get_header_induction_variable(l2);
      if (!l1_header_ind_var || !l2_header_ind_var) {
        return false;
      }

      l2_header_ind_var->replaceAllUsesWith(l1_header_ind_var);

      BasicBlock *l1_body_tail = get_body_tail(l1);
      if (!l1_body_tail) {
        return false;
      }

      cast<BranchInst>(l1_body_tail->getTerminator())->setSuccessor(0, l2_body_head);

      for (PHINode &phi : l2_body_head->phis()) {
        int i = phi.getBasicBlockIndex(l2_header);
        if (i != -1) {
          phi.setIncomingBlock(i, l1_body_tail);
        }
      }

      BasicBlock *l2_body_tail = get_body_tail(l2);
      if (!l2_body_tail) {
        return false;
      }

      cast<BranchInst>(l2_body_tail->getTerminator())->setSuccessor(0, l1_latch);

      for (PHINode &phi : l1_latch->phis()) {
        int i = phi.getBasicBlockIndex(l1_body_tail);
        if (i != -1) {
          phi.setIncomingBlock(i, l2_body_tail);
        }
      }

      {
        BranchInst *branch_inst = cast<BranchInst>(l1_header->getTerminator());
        for (int i = 0; i < branch_inst->getNumSuccessors(); ++i) {
          if (branch_inst->getSuccessor(i) == l1_exit_block) {
            branch_inst->setSuccessor(i, l2_exit_block);
            break;
          }
        }
      }

      for (PHINode &phi : l2_exit_block->phis()) {
        int i = phi.getBasicBlockIndex(l2_header);
        if (i != -1) {
          phi.setIncomingBlock(i, l1_header);
        }
      }

      SmallVector<BasicBlock *, 8> blocks_to_remove;
      blocks_to_remove.push_back(l2_latch);
      blocks_to_remove.push_back(l2_header);
      if (l2_preheader && l2_preheader != l1_exit_block) {
        blocks_to_remove.push_back(l2_preheader);
      }
      blocks_to_remove.push_back(l1_exit_block);

      for (BasicBlock *basic_block : blocks_to_remove) {
        basic_block->dropAllReferences();
      }
      for (BasicBlock *basic_block : blocks_to_remove) {
        basic_block->eraseFromParent();
      }

      for (BasicBlock *basic_block : l2->blocks()) {
        l1->addBasicBlockToLoop(basic_block, LI);
      }

      LI.erase(l2);

      errs() << "Loops successfully fused.\n";

      return true;
    }

    void add_all_subloops(Loop* loop, SmallVector<Loop *, 8>& vector) {
      auto subloops = loop->getSubLoops();
      for (auto iter = subloops.rbegin(); iter != subloops.rend(); iter++) {
        Loop* subloop = *iter;
        vector.push_back(subloop);
        add_all_subloops(subloop, vector);
      }
    }

    bool run_pass_iteration(Function &F, FunctionAnalysisManager &AM) {
      bool changed = false;

      LoopInfo          &LI  = AM.getResult<LoopAnalysis>(F);
      DominatorTree     &DT  = AM.getResult<DominatorTreeAnalysis>(F);
      PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
      ScalarEvolution   &SE  = AM.getResult<ScalarEvolutionAnalysis>(F);

      SmallVector<Loop *, 8> loops;
      for (auto iter = LI.rbegin(); iter != LI.rend(); iter++) {
        Loop* loop = *iter;
        loops.push_back(loop);
        add_all_subloops(loop, loops);
      }

      int i = 0;
      while (i < loops.size() - 1) {
        Loop* l1 = loops[i];
        Loop* l2 = loops[i+1];

        bool adjacent = are_loops_adjacent(l1, l2);
        bool same_iter = same_number_of_iterations(l1, l2, SE);
        bool control_flow_eq = are_loops_control_flow_equivalent(l1, l2, DT, PDT);
        bool no_neg_deps = no_neg_dist_dep(l1, l2, SE);
        if (!adjacent || !same_iter || !control_flow_eq || !no_neg_deps) {
          i++;
          continue;
        }

        if (fuse_loops(l1, l2, LI)) {
          i += 2;
          changed = true;
        } else {
          i++;
        }
      }

      return changed;
    }

    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
      errs() << "\e[94m\n";

      errs() << "Running the loop fusion pass in function: " << F.getName() << " ...\n";

      while (run_pass_iteration(F, AM)) {}

      errs() << "\e[0m\n";

      return PreservedAnalyses::all();
    }


    // Without isRequired returning true, this pass will be skipped for functions
    // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
    // all functions with optnone.
    static bool isRequired() { return true; }
  };
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getLoopFusionPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopFusionPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "loop-fusion-pass") {
                    FPM.addPass(LoopFusionPass());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize LoopFusionPass when added to the pass pipeline on the
// command line, i.e. via '-passes=loop-fusion-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLoopFusionPassPluginInfo();
}
