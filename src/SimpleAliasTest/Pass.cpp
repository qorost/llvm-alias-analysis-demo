

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
            errs() << "\nFunction: " << F.getName() << " :\n";
            this->AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
            SetVector<Value*> allocated_pointers;
            int index = 0;
            for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++) {
                errs() << "\t(" <<index << "): " <<"\t"<< *I << "\n";
                size_t asize = allocated_pointers.size();
                for (unsigned i =0 ;i< asize;i++) {
                    AliasResult aa = this->AA->alias(&*I,allocated_pointers[i]);
                    errs() <<"\t\tis a " << aliasAsStr(aa) << " of "<< allocated_pointers[i]->getName() << "("<< allocated_pointers[i]<< ")"  << "\n";
                }
                if(AllocaInst * AI = dyn_cast<AllocaInst>(&*I)) {
                    //errs() << "\tAlloca instruction: " << *AI << ":Operand0: "<< AI->getOperand(0)<<" "<< AI->getName();
                    if(AI->getType()->isPointerTy()) {
                        //errs() << " is a pointer";
                    }
                    errs()<<"\n";
                    allocated_pointers.insert(AI);
                }

                index += 1;
            }
            return true;
    }
};
}

char SimpleAliasTest::ID = 0;

static RegisterPass<SimpleAliasTest> X("simpletester","concept tester",
                                     false,
                                     false);
