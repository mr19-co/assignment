#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include <unordered_map>

using namespace llvm;

//-----------------------------------------------------------------------------
// LoopPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {
  // New PM implementation
  struct LoopPass: PassInfoMixin<LoopPass> {
    bool is_loop_invariant(Loop* loop, Instruction* inst) {
      if (!isa<BinaryOperator>(inst) && !isa<UnaryOperator>(inst)) {
        return false;
      }

      for (Value* op : inst->operand_values()) {
        if (!isa<Instruction>(op) && !isa<Constant>(op) && !isa<Argument>(op)) {
          return false;
        }

        if (Instruction* op_inst = dyn_cast<Instruction>(op)) {
          if (loop->contains(op_inst)) {
            return false;
          }
        }
      }

      return true;
    }

    std::set<BasicBlock*> get_loop_exit_blocks(Loop* loop) {
      std::set<BasicBlock*> res;

      for (BasicBlock* block : loop->getBlocks()) {
        for (BasicBlock* succ : successors(block)) {
          if (!loop->contains(succ)) {
            res.insert(succ);
          }
        }
      }

      return res;
    }

    bool dominates_all_exit_blocks(DominatorTree* DT, Loop* loop, Instruction* inst) {
      std::set<BasicBlock*> exit_blocks = get_loop_exit_blocks(loop);
      for (BasicBlock* exit_block : exit_blocks) {
        if (!DT->dominates(inst->getParent(), exit_block)) {
          return false;
        }
      }
      return true;
    }

    bool is_dead_outside_loop(Loop* loop, Instruction* inst) {
      for (User* user : inst->users()) {
        if (Instruction* user_instr = dyn_cast<Instruction>(user)) {
          if (!loop->contains(user_instr)) {
            return false;
          }
        }
      }

      return true;
    }

    bool dominates_all_its_users(DominatorTree* DT, Loop* loop, Instruction* inst) {
      BasicBlock* block = inst->getParent();
      for (User* user : inst->users()) {
        Instruction* user_inst = dyn_cast<Instruction>(user);
        if (user_inst == nullptr) {
          return false;
        }

        if (!loop->contains(user_inst)) {
          return false;
        }

        if (!DT->dominates(block, user_inst->getParent())) {
          return false;
        }
      }

      return true;
    }

    bool is_moveable(DominatorTree* DT, Loop* loop, Instruction* inst) {
      bool res = (
        is_loop_invariant(loop, inst) &&
        (dominates_all_exit_blocks(DT, loop, inst) || is_dead_outside_loop(loop, inst)) &&
        dominates_all_its_users(DT, loop, inst)
      );
      return res;
    }

    bool optimize_loop_recursive(DominatorTree* DT, LoopInfo* LI, Loop* loop) {
      bool changed = false;

      for (BasicBlock* block : loop->getBlocks()) {
        Instruction* instructions[std::distance(block->begin(), block->end())];
        int i = 0;
        for (Instruction &inst : *block) {
          instructions[i] = &inst;
          i++;
        }
        for (Instruction* inst : instructions) {
          if (is_moveable(DT, loop, inst)) {
            if (!loop->getLoopPreheader()) {
              simplifyLoop(loop, DT, LI, nullptr, nullptr, nullptr, true);
            }
            BasicBlock* preheader = loop->getLoopPreheader();
            inst->moveBefore(preheader->getTerminator());
            changed = true;
          }
        }
      }

      for (Loop* subloop: *loop) {
        if (optimize_loop_recursive(DT, LI, subloop)) {
          changed = true;
        }
      }

      return changed;
    }

    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

      bool changed;
      do {
        changed = false;
        for (Loop* loop : LI) {
          if (optimize_loop_recursive(&DT, &LI, loop)) {
            changed = true;
          }
        }
      } while (changed);

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
llvm::PassPluginLibraryInfo getLoopPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "loop-pass") {
                    FPM.addPass(LoopPass());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize LoopPass when added to the pass pipeline on the
// command line, i.e. via '-passes=loop-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLoopPassPluginInfo();
}
