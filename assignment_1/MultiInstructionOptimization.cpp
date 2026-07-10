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

    std::pair<Value*, int>* sumToConstant_Operands(Instruction* inst) {
      errs() << "        Finding variable and constant operands of (possible) algebraic sum " << *inst << "\n";
      BinaryOperator* bin_op = dyn_cast<BinaryOperator>(inst);
      if (bin_op == nullptr) {
        errs() << "            Instruction is not a binary operation, returning nullptr.\n";
        return nullptr;
      }
      if (!isa<AddOperator>(inst) && !isa<SubOperator>(inst)) {
        errs() << "            Instruction is not an addition or a subtraction, returning nullptr.\n";
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

      if (!constant_operand) {
        errs() << "            No integer constant operands or left operand is integer constant and operation is subtraction\n";
        return nullptr;
      }

      errs() << "            Variable operand: " << *variable_operand << "\n";
      errs() << "            Constant operand: " << *constant_operand << "\n";
      
      int constant_operand_value = 0;
      switch (bin_op->getOpcode()) {
        case BinaryOperator::Add:
          constant_operand_value = constant_operand->getSExtValue();
          break;
        case BinaryOperator::Sub:
          constant_operand_value = -constant_operand->getSExtValue();
          break;
      }

      errs() << "            Constant operand value: " << constant_operand_value << "\n";

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
        errs() << "\n";
        errs() << "    Attempting to optimize instructon " << inst << "\n";
        
        std::pair<Value*, int>* inst_operands = sumToConstant_Operands(&inst);
        if (inst_operands == nullptr) {
          errs() << "        Could not find operands: cannot optimize instruction\n";
          continue;
        }

        Instruction* used_inst = dyn_cast<Instruction>(inst_operands->first);
        if (used_inst == nullptr) {
          errs() << "        Could not find used instruction: cannot optimize instruction\n";
          continue;
        }

        std::pair<Value*, int>* used_inst_operands = sumToConstant_Operands(used_inst);
        if (used_inst_operands == nullptr) {
          errs() << "        Could not find used instruction operands: cannot optimize instruction\n";
          continue;
        }

        Value* new_inst_variable_operand = used_inst_operands->first;
        int new_inst_constant_operand_value = inst_operands->second + used_inst_operands->second;

        errs() << "        Optimized instruction varable operand: " << *new_inst_variable_operand << "\n";
        errs() << "        Optimized instruction constant operand with sign: " << new_inst_constant_operand_value << "\n";

        Instruction* new_inst;
        if (new_inst_constant_operand_value < 0) {
          ConstantInt* new_inst_constant_operand = builder.getInt32(-new_inst_constant_operand_value);
          new_inst = cast<Instruction>(builder.CreateSub(new_inst_variable_operand, new_inst_constant_operand));
        } else {
          ConstantInt* new_inst_constant_operand = builder.getInt32(new_inst_constant_operand_value);
          new_inst = cast<Instruction>(builder.CreateAdd(new_inst_variable_operand, new_inst_constant_operand));
        }
        
        new_inst->insertAfter(&inst);
        inst.replaceAllUsesWith(new_inst);

        errs() << "        Optimized instruction added: " << *new_inst << "\n";
      }
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
      errs() << "\e[94m\n";

      errs() << "Running multi-instruction optimization pass in function " << F.getName() << " ...\n";

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
