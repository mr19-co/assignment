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

    bool dominates_all_exit_blocks(DominatorTree* DT, Loop* loop, Instruction* inst) {
      SmallVector<BasicBlock*> exit_blocks;
      loop->getExitBlocks(exit_blocks);
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
      errs() << "                Checking if instruction " << *inst << "is moveable\n";

      bool inst_is_loop_invariant = is_loop_invariant(loop, inst);
      bool inst_dominates_all_exit_blocks = dominates_all_exit_blocks(DT, loop, inst);
      bool inst_is_dead_outside_loop = is_dead_outside_loop(loop, inst);
      bool inst_dominates_all_its_users = dominates_all_its_users(DT, loop, inst);

      errs() << "                    Loop invariant: " << (inst_is_loop_invariant ? "yes" : "no") << "\n";
      errs() << "                    Dominates all exit blocks: " << (inst_dominates_all_exit_blocks ? "yes" : "no") << "\n";
      errs() << "                    Dead outside loop: " << (inst_is_dead_outside_loop ? "yes" : "no") << "\n";
      errs() << "                    Dominates all its users: " << (inst_dominates_all_its_users ? "yes" : "no") << "\n";

      bool res = (
        inst_is_loop_invariant &&
        (inst_dominates_all_exit_blocks || inst_is_dead_outside_loop) &&
        inst_dominates_all_its_users
      );

      if (res) {
        errs() << "                Instruction is moveable\n";
      } else {
        errs() << "                Instruction is not moveable\n";
      }

      return res;
    }

    bool optimize_loop_recursive(DominatorTree* DT, LoopInfo* LI, Loop* loop) {
      errs() << "\n";
      errs() << "        Attempting to optimize loop with header labelled: ";
      loop->getHeader()->printAsOperand(errs());
      errs() << "\n";

      bool changed = false;

      for (BasicBlock* block : loop->getBlocks()) {
        Instruction* instructions[std::distance(block->begin(), block->end())];
        int i = 0;
        for (Instruction &inst : *block) {
          instructions[i] = &inst;
          i++;
        }
        for (Instruction* inst : instructions) {
          errs() << "\n";
          errs() << "            Attempting to move instruction " << *inst << "\n";
          if (is_moveable(DT, loop, inst)) {
            if (!loop->getLoopPreheader()) {
              errs() << "                Preheader not found, simplifying loop\n";
              simplifyLoop(loop, DT, LI, nullptr, nullptr, nullptr, true);
            }
            BasicBlock* preheader = loop->getLoopPreheader();
            inst->moveBefore(preheader->getTerminator());
            errs() << "                Instruction moved\n";
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
      errs() << "\e[94m\n";

      errs() << "Running loop-invariant code motion pass in function " << F.getName() << " ...\n";

      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

      bool changed;
      int i = 1;
      do {
        errs() << "    Iteration " << i << "\n";
        changed = false;
        for (Loop* loop : LI) {
          if (optimize_loop_recursive(&DT, &LI, loop)) {
            changed = true;
          }
        }
        i += 1;
      } while (changed);

      errs() << "\e[0m\n";

      return PreservedAnalyses::none();
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
