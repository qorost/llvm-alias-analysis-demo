

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


bool PrintAll = true;
bool PrintNoAlias = true;
bool PrintMayAlias = true;
bool PrintMustAlias = true;
bool PrintPartialAlias = true;
bool PrintNoModRef = true;
bool PrintMod = true;
bool PrintRef = true;
bool PrintModRef = true;
bool EvalAAMD = true;

static void PrintResults(const char *Msg, bool P, const Value *V1,
                         const Value *V2, const Module *M) {
  if (PrintAll || P) {
    std::string o1, o2;
    {
      raw_string_ostream os1(o1), os2(o2);
      V1->printAsOperand(os1, true, M);
      V2->printAsOperand(os2, true, M);
    }

    if (o2 < o1)
      std::swap(o1, o2);
    errs() << "  " << Msg << ":\t"
           << o1 << ", "
           << o2 << "\n";
  }
}

static inline void
PrintModRefResults(const char *Msg, bool P, Instruction *I, Value *Ptr,
                   Module *M) {
  if (PrintAll || P) {
    errs() << "  " << Msg << ":  Ptr: ";
    Ptr->printAsOperand(errs(), true, M);
    errs() << "\t<->" << *I << '\n';
  }
}

static inline void
PrintModRefResults(const char *Msg, bool P, CallSite CSA, CallSite CSB,
                   Module *M) {
  if (PrintAll || P) {
    errs() << "  " << Msg << ": " << *CSA.getInstruction()
           << " <-> " << *CSB.getInstruction() << '\n';
  }
}

static inline void
PrintLoadStoreResults(const char *Msg, bool P, const Value *V1,
                      const Value *V2, const Module *M) {
  if (PrintAll || P) {
    errs() << "  " << Msg << ": " << *V1
           << " <-> " << *V2 << '\n';
  }
}

static inline bool isInterestingPointer(Value *V) {
  return V->getType()->isPointerTy()
      && !isa<ConstantPointerNull>(V);
}


class SimpleEvaluator : public FunctionPass {
    public:
        static char ID;             // Pass identification, replacement for typeid
        SimpleEvaluator() : FunctionPass(ID) {}
        AliasAnalysis *AA;

        int64_t FunctionCount;
        int64_t NoAliasCount, MayAliasCount, PartialAliasCount, MustAliasCount;
        int64_t NoModRefCount, ModCount, RefCount, ModRefCount;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesAll();
            AU.addRequired<AAResultsWrapperPass>();
    }


    bool runOnFunction(Function &F) override {
        this->AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();


          const DataLayout &DL = F.getParent()->getDataLayout();

          ++FunctionCount;

          SetVector<Value *> Pointers;
          SmallSetVector<CallSite, 16> CallSites;
          SetVector<Value *> Loads;
          SetVector<Value *> Stores;

          for (auto &I : F.args())
            if (I.getType()->isPointerTy())    // Add all pointer arguments.
              Pointers.insert(&I);

          for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
            if (I->getType()->isPointerTy()) // Add all pointer instructions.
              Pointers.insert(&*I);
            if (EvalAAMD && isa<LoadInst>(&*I))
              Loads.insert(&*I);
            if (EvalAAMD && isa<StoreInst>(&*I))
              Stores.insert(&*I);
            Instruction &Inst = *I;
            if (auto CS = CallSite(&Inst)) {
              Value *Callee = CS.getCalledValue();
              // Skip actual functions for direct function calls.
              if (!isa<Function>(Callee) && isInterestingPointer(Callee))
                Pointers.insert(Callee);
              // Consider formals.
              for (Use &DataOp : CS.data_ops())
                if (isInterestingPointer(DataOp))
                  Pointers.insert(DataOp);
              CallSites.insert(CS);
            } else {
              // Consider all operands.
              for (Instruction::op_iterator OI = Inst.op_begin(), OE = Inst.op_end();
                   OI != OE; ++OI)
                if (isInterestingPointer(*OI))
                  Pointers.insert(*OI);
            }
          }

          if (PrintAll || PrintNoAlias || PrintMayAlias || PrintPartialAlias ||
              PrintMustAlias || PrintNoModRef || PrintMod || PrintRef || PrintModRef)
            errs() << "Function: " << F.getName() << ": " << Pointers.size()
                   << " pointers, " << CallSites.size() << " call sites\n";

          // iterate over the worklist, and run the full (n^2)/2 disambiguations
          for (SetVector<Value *>::iterator I1 = Pointers.begin(), E = Pointers.end();
               I1 != E; ++I1) {
            uint64_t I1Size = MemoryLocation::UnknownSize;
            Type *I1ElTy = cast<PointerType>((*I1)->getType())->getElementType();
            if (I1ElTy->isSized()) I1Size = DL.getTypeStoreSize(I1ElTy);

            for (SetVector<Value *>::iterator I2 = Pointers.begin(); I2 != I1; ++I2) {
              uint64_t I2Size = MemoryLocation::UnknownSize;
              Type *I2ElTy =cast<PointerType>((*I2)->getType())->getElementType();
              if (I2ElTy->isSized()) I2Size = DL.getTypeStoreSize(I2ElTy);

              switch (this->AA->alias(*I1, I1Size, *I2, I2Size)) {
              case NoAlias:
                PrintResults("NoAlias", PrintNoAlias, *I1, *I2, F.getParent());
                ++NoAliasCount;
                break;
              case MayAlias:
                PrintResults("MayAlias", PrintMayAlias, *I1, *I2, F.getParent());
                ++MayAliasCount;
                break;
              case PartialAlias:
                PrintResults("PartialAlias", PrintPartialAlias, *I1, *I2,
                             F.getParent());
                ++PartialAliasCount;
                break;
              case MustAlias:
                PrintResults("MustAlias", PrintMustAlias, *I1, *I2, F.getParent());
                ++MustAliasCount;
                break;
              }
            }
          }

          if (EvalAAMD) {
            // iterate over all pairs of load, store
            for (Value *Load : Loads) {
              for (Value *Store : Stores) {
                switch (this->AA->alias(MemoryLocation::get(cast<LoadInst>(Load)),
                                 MemoryLocation::get(cast<StoreInst>(Store)))) {
                case NoAlias:
                  PrintLoadStoreResults("NoAlias", PrintNoAlias, Load, Store,
                                        F.getParent());
                  ++NoAliasCount;
                  break;
                case MayAlias:
                  PrintLoadStoreResults("MayAlias", PrintMayAlias, Load, Store,
                                        F.getParent());
                  ++MayAliasCount;
                  break;
                case PartialAlias:
                  PrintLoadStoreResults("PartialAlias", PrintPartialAlias, Load, Store,
                                        F.getParent());
                  ++PartialAliasCount;
                  break;
                case MustAlias:
                  PrintLoadStoreResults("MustAlias", PrintMustAlias, Load, Store,
                                        F.getParent());
                  ++MustAliasCount;
                  break;
                }
              }
            }

            // iterate over all pairs of store, store
            for (SetVector<Value *>::iterator I1 = Stores.begin(), E = Stores.end();
                 I1 != E; ++I1) {
              for (SetVector<Value *>::iterator I2 = Stores.begin(); I2 != I1; ++I2) {
                switch (this->AA->alias(MemoryLocation::get(cast<StoreInst>(*I1)),
                                 MemoryLocation::get(cast<StoreInst>(*I2)))) {
                case NoAlias:
                  PrintLoadStoreResults("NoAlias", PrintNoAlias, *I1, *I2,
                                        F.getParent());
                  ++NoAliasCount;
                  break;
                case MayAlias:
                  PrintLoadStoreResults("MayAlias", PrintMayAlias, *I1, *I2,
                                        F.getParent());
                  ++MayAliasCount;
                  break;
                case PartialAlias:
                  PrintLoadStoreResults("PartialAlias", PrintPartialAlias, *I1, *I2,
                                        F.getParent());
                  ++PartialAliasCount;
                  break;
                case MustAlias:
                  PrintLoadStoreResults("MustAlias", PrintMustAlias, *I1, *I2,
                                        F.getParent());
                  ++MustAliasCount;
                  break;
                }
              }
            }
          }

          // Mod/ref alias analysis: compare all pairs of calls and values
          for (CallSite C : CallSites) {
            Instruction *I = C.getInstruction();

            for (auto Pointer : Pointers) {
              uint64_t Size = MemoryLocation::UnknownSize;
              Type *ElTy = cast<PointerType>(Pointer->getType())->getElementType();
              if (ElTy->isSized()) Size = DL.getTypeStoreSize(ElTy);

              switch (this->AA->getModRefInfo(C, Pointer, Size)) {
              case MRI_NoModRef:
                PrintModRefResults("NoModRef", PrintNoModRef, I, Pointer,
                                   F.getParent());
                ++NoModRefCount;
                break;
              case MRI_Mod:
                PrintModRefResults("Just Mod", PrintMod, I, Pointer, F.getParent());
                ++ModCount;
                break;
              case MRI_Ref:
                PrintModRefResults("Just Ref", PrintRef, I, Pointer, F.getParent());
                ++RefCount;
                break;
              case MRI_ModRef:
                PrintModRefResults("Both ModRef", PrintModRef, I, Pointer,
                                   F.getParent());
                ++ModRefCount;
                break;
              }
            }
          }

          // Mod/ref alias analysis: compare all pairs of calls
          for (auto C = CallSites.begin(), Ce = CallSites.end(); C != Ce; ++C) {
            for (auto D = CallSites.begin(); D != Ce; ++D) {
              if (D == C)
                continue;
              switch (this->AA->getModRefInfo(*C, *D)) {
              case MRI_NoModRef:
                PrintModRefResults("NoModRef", PrintNoModRef, *C, *D, F.getParent());
                ++NoModRefCount;
                break;
              case MRI_Mod:
                PrintModRefResults("Just Mod", PrintMod, *C, *D, F.getParent());
                ++ModCount;
                break;
              case MRI_Ref:
                PrintModRefResults("Just Ref", PrintRef, *C, *D, F.getParent());
                ++RefCount;
                break;
              case MRI_ModRef:
                PrintModRefResults("Both ModRef", PrintModRef, *C, *D, F.getParent());
                ++ModRefCount;
                break;
              }
            }
          }
          return true;
        }
};
}

char SimpleEvaluator::ID = 0;

static RegisterPass<SimpleEvaluator> X("simpleevaluator","simple evaluator",
                                     false,
                                     false);
