#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/PostOrderIterator.h"

#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/SetVector.h"

#include "llvm/Analysis/AliasAnalysis.h"


//for std
#include <vector>
using namespace llvm;

namespace {


StringRef dropkeyword = StringRef("drop.");

const char* aliasAsStr(AliasResult a){
        switch(a) {
        case AliasResult::MustAlias:
                return "MustAlias";
        case AliasResult::PartialAlias:
                return "PartialAlias";
        case AliasResult::MayAlias:
                return "MayAlias";
        case AliasResult::NoAlias:
        default:
                return "NoAlias";
        }
}

static const Function *getParent(const Value *V) {
    if(const Instruction *inst = dyn_cast<Instruction> (V)) {
        return inst->getParent()->getParent();
    }
    if(const Argument *arg = dyn_cast<Argument> (V)) {
        return arg->getParent();
    }
    return nullptr;
}


class SimpleAliasTest : public FunctionPass {
    public:
        static char ID;             // Pass identification, replacement for typeid
        SimpleAliasTest() : FunctionPass(ID) {}
        AliasAnalysis *AA;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesAll();
            AU.addRequired<AAResultsWrapperPass>();
    }


    bool runOnFunction(Function &F) override {
            errs() << "\nDealing with Function: " << F.getName() << " :\n";
            this->AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
            SetVector<Value*> dropped_pointers ;
            //BasicBlock& firstbb = F.getEntryBlock();
            //transCFG(&firstbb,0,dropped_pointers);
            //iterate over the lines of the code, deteced the allocated variables aliasing
            SetVector<Value*> allocated_pointers;

            for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++) {
                if(AllocaInst * AI = dyn_cast<AllocaInst>(&*I)) {
                    errs() << "\tAlloc instruction: " << *AI << ":Operand0: "<< AI->getOperand(0)<<" "<< AI->getName()<<"\n";
                    size_t asize = allocated_pointers.size();
                    for (int i =0 ;i< asize;i++) {
                        AliasResult aa = this->AA->alias(AI,allocated_pointers[i]);
                        errs() << *AI << " is a " << aliasAsStr(aa) << " of " << allocated_pointers[i] << "\n";
                    }
                    allocated_pointers.insert(AI);
                }
            }
            return true;
    }
};
}

char SimpleAliasTest::ID = 0;

static RegisterPass<SimpleAliasTest> X("simpletester","concept tester",
                                     false,
                                     false);
