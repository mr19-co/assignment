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
  struct MultiInstructionOptimization: PassInfoMixin<MultiInstructionOptimization> {

    std::pair<Value*, int>* sumToConstantOperands(Instruction* inst) {
      BinaryOperator* bin_op = dyn_cast<BinaryOperator>(inst);
      if (bin_op == nullptr) {
        return nullptr;
      }
      
      Value* variable_operand = nullptr;
      ConstantInt* constant_operand = nullptr;
      if (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(1))) {
        variable_operand = bin_op->getOperand(0);
      } else {
        if (
          (bin_op->getOpcode() == BinaryOperator::Add) &&
          (constant_operand = dyn_cast<ConstantInt>(bin_op->getOperand(0)))
        ) {
          variable_operand = bin_op->getOperand(1);
        }
      }

      if (variable_operand == nullptr) {
        return nullptr;
      }
      
      int constant_operand_value = 0;
      switch (bin_op->getOpcode()) {
        case BinaryOperator::Add:
          constant_operand_value = constant_operand->getSExtValue();
          break;
        case BinaryOperator::Sub:
          constant_operand_value = -constant_operand->getSExtValue();
          break;
        default:
          return nullptr;
      }

      std::pair<Value*, int>* res = new std::pair<Value*, int>();
      res->first = variable_operand;
      res->second = constant_operand_value;
      return res;
    }
    
    void runOnBasicBlock(BasicBlock &B) {
      LLVMContext &context = B.getContext();
      IRBuilder<> builder(context);

      for (BasicBlock::iterator iter = B.begin(); iter != B.end(); iter++) {
        Instruction &inst = *iter;
        
        std::pair<Value*, int>* inst_operands = sumToConstantOperands(&inst);
        if (inst_operands == nullptr) { continue; }

        Instruction* used_inst = dyn_cast<Instruction>(inst_operands->first);
        if (used_inst == nullptr) { continue; }

        std::pair<Value*, int>* used_inst_operands = sumToConstantOperands(used_inst);
        if (used_inst_operands == nullptr) { continue; }

        Value* new_inst_variable_operand = used_inst_operands->first;
        int new_inst_constant_operand_value = inst_operands->second + used_inst_operands->second;
        Instruction* new_inst;
        if (new_inst_constant_operand_value < 0) {
          ConstantInt* new_inst_constant_operand = builder.getInt32(-new_inst_constant_operand_value);
          new_inst = dyn_cast<Instruction>(builder.CreateSub(new_inst_variable_operand, new_inst_constant_operand));
        } else {
          ConstantInt* new_inst_constant_operand = builder.getInt32(new_inst_constant_operand_value);
          new_inst = dyn_cast<Instruction>(builder.CreateAdd(new_inst_variable_operand, new_inst_constant_operand));
        }
        
        new_inst->insertAfter(&inst);
        inst.replaceAllUsesWith(new_inst);
      }
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
llvm::PassPluginLibraryInfo getMultiInstructionOptimizationPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MultiInstructionOptimization", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "multi-instruction-optimization") {
                    FPM.addPass(MultiInstructionOptimization());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMultiInstructionOptimizationPluginInfo();
}
