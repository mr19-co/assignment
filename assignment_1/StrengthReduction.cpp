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
  struct StrengthReduction: PassInfoMixin<StrengthReduction> {
    
    bool runOnBasicBlock(BasicBlock &B) {
      LLVMContext &context = B.getContext();
      IRBuilder<> builder(context);

      for (BasicBlock::iterator iter = B.begin(); iter != B.end(); iter++) {
        Instruction &inst = *iter;
        
        if (BinaryOperator* bin_op = dyn_cast<BinaryOperator>(&inst)) {
          bool change = false;
          Value* variable_operand = nullptr;
          ConstantInt* constant_operand = nullptr;
          if (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(1))) {
            variable_operand = bin_op->getOperand(0);
            change = true;
          } else {
            if (
              (bin_op->getOpcode() == BinaryOperator::Mul) &&
              (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(0)))
            ) {
              variable_operand = bin_op->getOperand(1);
              change = true;
            }
          }

          int exp = 0;
          int diff = 0;
          if (change) {
            int v = constant_operand->getValue().getSExtValue();
            if (v > 1) {
              int pow = 1;
              while (pow < v) {
                exp++;
                pow *= 2;
              }

              if (v == pow - 1) {
                diff = -1;
              }
              if (v == pow / 2 + 1) {
                pow /= 2;
                exp--;
                diff = 1;
              }

              if (v != pow + diff) {
                change = false;
              }
            } else {
              change = false;
            }
          }

          if (change) {
            Instruction* shift_inst = nullptr;
            Instruction* add_or_sub_inst = nullptr;

            switch(bin_op->getOpcode()) {
              case BinaryOperator::Mul:
                shift_inst = dyn_cast<Instruction>(builder.CreateShl(variable_operand, exp));
                shift_inst->insertAfter(&inst);
                
                if (diff == 0) {
                  inst.replaceAllUsesWith(shift_inst);
                } else {
                  add_or_sub_inst = nullptr;
                  switch (diff) {
                    case -1:
                      add_or_sub_inst = dyn_cast<Instruction>(builder.CreateSub(shift_inst, variable_operand));
                      break;
                    case 1:
                      add_or_sub_inst = dyn_cast<Instruction>(builder.CreateAdd(shift_inst, variable_operand));
                      break;
                  }
                  add_or_sub_inst->insertAfter(shift_inst);
                  inst.replaceAllUsesWith(add_or_sub_inst);
                }
                
                break;
              case BinaryOperator::UDiv:
                shift_inst = dyn_cast<Instruction>(builder.CreateLShr(variable_operand, exp));
                shift_inst->insertAfter(&inst);
                inst.replaceAllUsesWith(shift_inst);
                break;
              case BinaryOperator::SDiv:
                shift_inst = dyn_cast<Instruction>(builder.CreateAShr(variable_operand, exp));
                shift_inst->insertAfter(&inst);
                inst.replaceAllUsesWith(shift_inst);
                break;
            }
          }
        }
      }

      return true;
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
      for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
        runOnBasicBlock(*Iter);
      }
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
llvm::PassPluginLibraryInfo getStrengthReductionPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StrengthReduction", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "strength-reduction") {
                    FPM.addPass(StrengthReduction());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getStrengthReductionPluginInfo();
}
