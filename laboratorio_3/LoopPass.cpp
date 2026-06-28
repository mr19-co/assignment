#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"

using namespace llvm;

//-----------------------------------------------------------------------------
// LoopPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

  int countFunctionUses(Function* F) {
    int res = 0;

    for (User* U : F->users()) {
      if (CallBase* callInst = dyn_cast<CallBase>(U)) {
        if (callInst->getCalledFunction() == F) {
          res++;
        }
      }
    }

    return res;
  }

  // New PM implementation
  struct LoopPass: PassInfoMixin<LoopPass> {
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
      errs() << "The function's dominator tree is:\n";
      DT.print(errs());
      errs() << '--------------------------------------\n';

      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
      if (LI.begin() == LI.end()) {
        errs() << "CFG does not contain any loops.\n";
        errs() << "======================================\n";
        return PreservedAnalyses::all();
      }

      for (BasicBlock &BB : F) {
        errs() << "Basic Block " << BB.getName() << " is a loop header\n";
      }

      errs() << '--------------------------------------\n';

      for (Loop *L : LI.getLoopsInPreorder()) {
        errs() << "Analysing loop " << L->getName() << "\n";
        errs() << "\n";

        if (L->isLoopSimplifyForm()) {
          errs() << "The loop is in normal form.";
        } else {
          errs() << "The loop is not in normal form.";
        }
        errs() << "\n";

        Function* loop_function = L->getHeader()->getParent();
        errs() << "The CFG of the loop's function is:\n";
        loop_function->print(errs());
        errs() << "\n";

        errs() << "The loop's blocks are:\n";
        for (BasicBlock* BB : L->getBlocks()) {
          BB->print(errs());
          errs() << "\n";
        }

        errs() << "======================================\n";
      }

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
