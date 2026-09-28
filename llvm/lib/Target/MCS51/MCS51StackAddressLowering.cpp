//===-- MCS51StackAddressLowering.cpp - MCS-51 indexed stack addresses ----===//

#include "MCS51TargetMachine.h"
#include "MCS51.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
class MCS51StackAddressLowering final : public FunctionPass {
public:
  static char ID;
  MCS51StackAddressLowering() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    SmallVector<AllocaInst *, 8> Allocations;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *AI = dyn_cast<AllocaInst>(&I))
          Allocations.push_back(AI);

    bool Changed = false;
    for (AllocaInst *AI : Allocations)
      Changed |= lowerIndexedStackObject(*AI, LI);
    return Changed;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
  }

private:
  static bool lowerIndexedStackObject(AllocaInst &AI, LoopInfo &LI) {
    SmallVector<Value *, 16> Worklist{&AI};
    SmallPtrSet<Value *, 16> Seen;
    SmallVector<GetElementPtrInst *, 8> GEPs;
    bool HasVariableIndex = false;

    while (!Worklist.empty()) {
      Value *Pointer = Worklist.pop_back_val();
      if (!Seen.insert(Pointer).second)
        continue;
      for (User *U : Pointer->users()) {
        if (isa<DbgInfoIntrinsic>(U) || isa<LifetimeIntrinsic>(U))
          continue;
        if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
          if (GEP->getPointerOperand() != Pointer)
            return false;
          GEPs.push_back(GEP);
          for (Value *Index : GEP->indices())
            HasVariableIndex |= !isa<ConstantInt>(Index) &&
                                LI.getLoopFor(GEP->getParent());
          Worklist.push_back(GEP);
          continue;
        }
        if (auto *Load = dyn_cast<LoadInst>(U)) {
          if (Load->getPointerOperand() == Pointer)
            continue;
        }
        if (auto *Store = dyn_cast<StoreInst>(U))
          if (Store->getPointerOperand() == Pointer)
            continue;
        // Keeping pointer escapes and other pointer operations in their
        // original address space avoids changing their observable behavior.
        return false;
      }
    }
    if (!HasVariableIndex)
      return false;

    LLVMContext &C = AI.getContext();
    Type *IDataPointerTy = PointerType::get(C, MCS51::IData);
    IRBuilder<> Builder(AI.getNextNode());
    Value *BaseCast = Builder.CreateAddrSpaceCast(
        &AI, IDataPointerTy, AI.getName() + ".idata");
    DenseMap<Value *, Value *> Remapped;
    Remapped[&AI] = BaseCast;

    for (GetElementPtrInst *GEP : GEPs) {
      Value *NewBase = Remapped.lookup(GEP->getPointerOperand());
      if (!NewBase)
        return false;
      SmallVector<Value *, 4> Indices;
      for (Value *Index : GEP->indices())
        Indices.push_back(Index);
      IRBuilder<> GEPBuilder(GEP);
      auto *NewGEP = cast<GetElementPtrInst>(GEPBuilder.CreateGEP(
          GEP->getSourceElementType(), NewBase, Indices,
          GEP->getName() + ".idata"));
      NewGEP->setIsInBounds(GEP->isInBounds());
      NewGEP->setNoWrapFlags(GEP->getNoWrapFlags());
      NewGEP->setDebugLoc(GEP->getDebugLoc());
      Remapped[GEP] = NewGEP;
    }

    for (Value *Pointer : Seen) {
      Value *NewPointer = Remapped.lookup(Pointer);
      if (!NewPointer)
        continue;
      for (User *U : Pointer->users()) {
        if (auto *Load = dyn_cast<LoadInst>(U)) {
          if (Load->getPointerOperand() == Pointer)
            Load->setOperand(Load->getPointerOperandIndex(), NewPointer);
        } else if (auto *Store = dyn_cast<StoreInst>(U)) {
          if (Store->getPointerOperand() == Pointer)
            Store->setOperand(Store->getPointerOperandIndex(), NewPointer);
        }
      }
    }

    for (auto It = GEPs.rbegin(); It != GEPs.rend(); ++It)
      if ((*It)->use_empty())
        (*It)->eraseFromParent();
    return true;
  }
};

char MCS51StackAddressLowering::ID = 0;
} // namespace

FunctionPass *llvm::createMCS51StackAddressLoweringPass() {
  return new MCS51StackAddressLowering();
}
