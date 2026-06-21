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
  struct AlgebraicIdentity: PassInfoMixin<AlgebraicIdentity> {
    
    bool runOnBasicBlock(BasicBlock &B) {
      LLVMContext &context = B.getContext();
      IRBuilder<> builder(context);

      for (BasicBlock::iterator iter = B.begin(); iter != B.end(); iter++) {
        Instruction &inst = *iter;
        
        if (BinaryOperator* bin_op = dyn_cast<BinaryOperator>(&inst)) {
          Value* variable_operand = nullptr;
          ConstantInt* constant_operand = nullptr;
          if (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(1))) {
            variable_operand = bin_op->getOperand(0);
          } else {
            if (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(0))) {
              variable_operand = bin_op->getOperand(1);
            }
          }

          switch(bin_op->getOpcode()) {
            case BinaryOperator::Add:
              if (constant_operand != nullptr && constant_operand->getValue().getSExtValue() == 0) {
                inst.replaceAllUsesWith(variable_operand);
              }
              break;
            case BinaryOperator::Mul:
              if (constant_operand != nullptr && constant_operand->getValue().getSExtValue() == 1) {
                inst.replaceAllUsesWith(variable_operand);
              }
              break;
          }
        }
      }

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
llvm::PassPluginLibraryInfo getAlgebraicIdentityPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentity", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "algebraic-identity") {
                    FPM.addPass(AlgebraicIdentity());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getAlgebraicIdentityPluginInfo();
}
