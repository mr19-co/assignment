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
    
    void runOnBasicBlock(BasicBlock &B) {
      LLVMContext &context = B.getContext();
      IRBuilder<> builder(context);

      for (BasicBlock::iterator iter = B.begin(); iter != B.end(); iter++) {
        Instruction &inst = *iter;
        errs() << "\n";
        errs() << "    Attempting to optimize instructon " << inst << "\n";
        
        if (!isa<BinaryOperator>(&inst)) {
          errs() << "        Instruction is not binary operation.\n";
          continue;
        }

        BinaryOperator* bin_op = cast<BinaryOperator>(&inst);

        if (!isa<AddOperator>(bin_op) && !isa<MulOperator>(bin_op)) {
          errs() << "            Instruction is not an addition or a multiplication, cannot perform optimization\n";
          continue;
        }

        Value* variable_operand = nullptr;
        ConstantInt* constant_operand = nullptr;
        if (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(1))) {
          variable_operand = bin_op->getOperand(0);
        } else {
          if (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(0))) {
            variable_operand = bin_op->getOperand(1);
          }
        }

        if (!constant_operand) {
          errs() << "        Instruction does not have any integer constant operands, cannot perform optimization.\n";
          continue;
        }

        errs() << "        Variable operand: " << *variable_operand << "\n";
        errs() << "        Constant operand: " << *constant_operand << "\n";

        switch(bin_op->getOpcode()) {
          case BinaryOperator::Add:
            if (constant_operand != nullptr && constant_operand->getValue().getSExtValue() == 0) {
              inst.replaceAllUsesWith(variable_operand);
              errs() << "        Operation is addition and constant operand is 0, optimization performed\n";
            } else {
              errs() << "        Operation is addition and constant operand is not 0, cannot perform optimization\n";
            }
            break;
          case BinaryOperator::Mul:
            if (constant_operand != nullptr && constant_operand->getValue().getSExtValue() == 1) {
              inst.replaceAllUsesWith(variable_operand);
              errs() << "        Operation is multiplication and constant operand is 1, optimization performed\n";
            } else {
              errs() << "        Operation is multiplication and constant operand is not 1, cannot perform optimization\n";
            }
            break;
        }
      }
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
      errs() << "\e[94m\n";

      errs() << "Running algebraic identity pass in function " << F.getName() << " ...\n";

      for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
        runOnBasicBlock(*Iter);
      }

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
