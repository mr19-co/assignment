//=============================================================================
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;


namespace {

  // New PM implementation
  struct LocalOpts: PassInfoMixin<LocalOpts> {
    
    bool runOnBasicBlock(BasicBlock &B) {
    
      // Preleviamo le prime due istruzioni del BB
      Instruction &Inst1st = *B.begin(), &Inst2nd = *(++B.begin());

      // L'indirizzo della prima istruzione deve essere uguale a quello del 
      // primo operando della seconda istruzione (per costruzione dell'esempio)
      assert(&Inst1st == Inst2nd.getOperand(0));

      // Stampa la prima istruzione
      outs() << "PRIMA ISTRUZIONE: " << Inst1st << "\n";
      // Stampa la prima istruzione come operando
      outs() << "COME OPERANDO: ";
      Inst1st.printAsOperand(outs(), false);
      outs() << "\n";

      // User-->Use-->Value
      outs() << "I MIEI OPERANDI SONO:\n";
      for (auto *Iter = Inst1st.op_begin(); Iter != Inst1st.op_end(); ++Iter) {
        Value *Operand = *Iter;

        if (Argument *Arg = dyn_cast<Argument>(Operand)) {
          outs() << "\t" << *Arg << ": SONO L'ARGOMENTO N. " << Arg->getArgNo() 
          <<" DELLA FUNZIONE " << Arg->getParent()->getName()
                << "\n";
        }
        if (ConstantInt *C = dyn_cast<ConstantInt>(Operand)) {
          outs() << "\t" << *C << ": SONO UNA COSTANTE INTERA DI VALORE " << C->getValue()
                << "\n";
        }
      }

      outs() << "LA LISTA DEI MIEI USERS:\n";
      for (auto Iter = Inst1st.user_begin(); Iter != Inst1st.user_end(); ++Iter) {
        outs() << "\t" << *(dyn_cast<Instruction>(*Iter)) << "\n";
      }

      outs() << "E DEI MIEI USI (CHE E' LA STESSA):\n";
      for (auto Iter = Inst1st.use_begin(); Iter != Inst1st.use_end(); ++Iter) {
        outs() << "\t" << *(dyn_cast<Instruction>(Iter->getUser())) << "\n";
      }

      // Manipolazione delle istruzioni
      Instruction *NewInst = BinaryOperator::Create(
          Instruction::Add, Inst1st.getOperand(0), Inst1st.getOperand(0));

      NewInst->insertAfter(&Inst1st);
      // Si possono aggiornare le singole references separatamente?
      // Controlla la documentazione e prova a rispondere.
      Inst1st.replaceAllUsesWith(NewInst);

      return true;
    }


    bool runOnFunction(Function &F) {
      bool Transformed = false;

      for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
        if (runOnBasicBlock(*Iter)) {
          Transformed = true;
        }
      }

      return Transformed;
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
      runOnFunction(F);
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
llvm::PassPluginLibraryInfo getLocalOptsPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LocalOpts", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "local-opts") {
                    FPM.addPass(LocalOpts());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLocalOptsPluginInfo();
}
