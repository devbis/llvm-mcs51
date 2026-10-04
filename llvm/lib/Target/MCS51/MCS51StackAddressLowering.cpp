//===-- MCS51StackAddressLowering.cpp - MCS-51 indexed stack addresses ----===//

#include "MCS51TargetMachine.h"
#include "MCS51.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

namespace {
class MCS51StackAddressLowering final : public FunctionPass {
public:
  static char ID;
  MCS51StackAddressLowering() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    SmallVector<AllocaInst *, 8> Allocations;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *AI = dyn_cast<AllocaInst>(&I))
          Allocations.push_back(AI);

    bool Changed = false;
    for (AllocaInst *AI : Allocations)
      Changed |= lowerIndexedStackObject(*AI);
    return Changed;
  }

private:
  static bool lowerIndexedStackObject(AllocaInst &AI) {
    SmallVector<Value *, 16> Worklist{&AI};
    SmallPtrSet<Value *, 16> Seen;
    SmallPtrSet<MemIntrinsic *, 4> MemCalls;
    SmallPtrSet<VAStartInst *, 2> VAStarts;
    SmallPtrSet<VAEndInst *, 2> VAEnds;
    SmallVector<VAArgInst *, 2> VAArgs;
    SmallVector<AddrSpaceCastInst *, 4> BackCasts;
    SmallVector<GetElementPtrInst *, 8> GEPs;
    bool HasVariableIndex = false;

    while (!Worklist.empty()) {
      Value *Pointer = Worklist.pop_back_val();
      if (!Seen.insert(Pointer).second)
        continue;
      for (User *U : Pointer->users()) {
        if (isa<DbgInfoIntrinsic>(U) || isa<LifetimeIntrinsic>(U))
          continue;
        if (auto *Cast = dyn_cast<AddrSpaceCastInst>(U)) {
          if (Cast->getPointerOperand() != Pointer)
            return false;
          // A cast back to the object's own address space is the identity on
          // the remapped pointer, whatever its users do with it.
          if (Cast->getDestAddressSpace() == AI.getAddressSpace()) {
            BackCasts.push_back(Cast);
            continue;
          }
          // Only the default address space is an access to the local; other
          // casts are escapes and keep their meaning.
          if (Cast->getDestAddressSpace() == MCS51::Default)
            Worklist.push_back(Cast);
          continue;
        }
        if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
          if (GEP->getPointerOperand() != Pointer)
            return false;
          GEPs.push_back(GEP);
          for (Value *Index : GEP->indices())
            HasVariableIndex |= !isa<ConstantInt>(Index);
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
        if (auto *VA = dyn_cast<VAStartInst>(U)) {
          if (VA->getArgList() == Pointer) {
            VAStarts.insert(VA);
            continue;
          }
        }
        if (auto *VA = dyn_cast<VAArgInst>(U)) {
          if (VA->getPointerOperand() == Pointer) {
            VAArgs.push_back(VA);
            continue;
          }
        }
        if (auto *VA = dyn_cast<VAEndInst>(U)) {
          if (VA->getArgList() == Pointer) {
            VAEnds.insert(VA);
            continue;
          }
        }
        if (auto *Mem = dyn_cast<MemIntrinsic>(U)) {
          // Block copies and fills accept pointers of any address space.
          bool OtherIsPointer = false;
          if (auto *Transfer = dyn_cast<MemTransferInst>(Mem))
            OtherIsPointer = Transfer->getRawSource() == Pointer &&
                             Transfer->getRawDest() == Pointer;
          if (!OtherIsPointer) {
            MemCalls.insert(Mem);
            continue;
          }
        }
        // Pointer escapes and other pointer operations keep using the
        // original pointer, which preserves their observable behavior; only
        // the accesses recognized above are redirected to IDATA.
      }
    }
    // Locals reached through casts to the default address space would be
    // accessed as XDATA; access them directly in IDATA, where they live. This
    // also lets mem2reg promote them.
    if (!HasVariableIndex && Seen.size() == 1)
      return false;

    LLVMContext &C = AI.getContext();
    Type *IDataPointerTy = PointerType::get(C, MCS51::IData);
    IRBuilder<> Builder(AI.getNextNode());
    Value *BaseCast = Builder.CreateAddrSpaceCast(
        &AI, IDataPointerTy, AI.getName() + ".idata");
    DenseMap<Value *, Value *> Remapped;
    Remapped[&AI] = BaseCast;
    for (Value *Pointer : Seen)
      if (auto *Cast = dyn_cast<AddrSpaceCastInst>(Pointer))
        if (Cast->getPointerOperand() == &AI)
          Remapped[Cast] = BaseCast;

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

    for (Value *Pointer : Seen)
      if (auto *Cast = dyn_cast<AddrSpaceCastInst>(Pointer))
        if (Value *NewBase = Remapped.lookup(Cast->getPointerOperand()))
          Remapped[Cast] = NewBase;

    for (Value *Pointer : Seen) {
      Value *NewPointer = Remapped.lookup(Pointer);
      if (!NewPointer)
        continue;
      SmallVector<User *, 8> PointerUsers(Pointer->users());
      for (User *U : PointerUsers) {
        if (auto *Load = dyn_cast<LoadInst>(U)) {
          if (Load->getPointerOperand() == Pointer)
            Load->setOperand(Load->getPointerOperandIndex(), NewPointer);
        } else if (auto *Store = dyn_cast<StoreInst>(U)) {
          if (Store->getPointerOperand() == Pointer)
            Store->setOperand(Store->getPointerOperandIndex(), NewPointer);
        }
      }
    }

    for (AddrSpaceCastInst *Cast : BackCasts)
      if (Value *New = Remapped.lookup(Cast->getPointerOperand())) {
        Cast->replaceAllUsesWith(New);
        Cast->eraseFromParent();
      }
    for (VAArgInst *VA : VAArgs) {
      Value *New = Remapped.lookup(VA->getPointerOperand());
      if (!New)
        continue;
      auto *NewVA = new VAArgInst(New, VA->getType(), VA->getName(), VA->getIterator());
      NewVA->setDebugLoc(VA->getDebugLoc());
      VA->replaceAllUsesWith(NewVA);
      VA->eraseFromParent();
    }
    for (VAStartInst *VA : VAStarts) {
      Value *New = Remapped.lookup(VA->getArgList());
      if (!New)
        continue;
      IRBuilder<> CallBuilder(VA);
      CallBuilder.SetCurrentDebugLocation(VA->getDebugLoc());
      CallBuilder.CreateIntrinsic(Intrinsic::vastart, {New->getType()}, {New});
      VA->eraseFromParent();
    }
    for (VAEndInst *VA : VAEnds) {
      Value *New = Remapped.lookup(VA->getArgList());
      if (!New)
        continue;
      IRBuilder<> CallBuilder(VA);
      CallBuilder.SetCurrentDebugLocation(VA->getDebugLoc());
      CallBuilder.CreateIntrinsic(Intrinsic::vaend, {New->getType()}, {New});
      VA->eraseFromParent();
    }
    for (MemIntrinsic *Mem : MemCalls) {
      auto Remap = [&](Value *V) {
        Value *New = Remapped.lookup(V);
        return New ? New : V;
      };
      IRBuilder<> CallBuilder(Mem);
      Instruction *NewCall = nullptr;
      if (auto *Copy = dyn_cast<MemCpyInst>(Mem))
        NewCall = CallBuilder.CreateMemCpy(
            Remap(Copy->getRawDest()), Copy->getDestAlign(),
            Remap(Copy->getRawSource()), Copy->getSourceAlign(),
            Copy->getLength(), Copy->isVolatile());
      else if (auto *Move = dyn_cast<MemMoveInst>(Mem))
        NewCall = CallBuilder.CreateMemMove(
            Remap(Move->getRawDest()), Move->getDestAlign(),
            Remap(Move->getRawSource()), Move->getSourceAlign(),
            Move->getLength(), Move->isVolatile());
      else if (auto *Set = dyn_cast<MemSetInst>(Mem))
        NewCall = CallBuilder.CreateMemSet(
            Remap(Set->getRawDest()), Set->getValue(), Set->getLength(),
            Set->getDestAlign(), Set->isVolatile());
      if (NewCall) {
        NewCall->setDebugLoc(Mem->getDebugLoc());
        Mem->eraseFromParent();
      }
    }
    for (auto It = GEPs.rbegin(); It != GEPs.rend(); ++It)
      if ((*It)->use_empty())
        (*It)->eraseFromParent();
    for (Value *Pointer : Seen)
      if (auto *Cast = dyn_cast<AddrSpaceCastInst>(Pointer))
        if (Cast->use_empty())
          Cast->eraseFromParent();
    return true;
  }
};

char MCS51StackAddressLowering::ID = 0;
} // namespace

FunctionPass *llvm::createMCS51StackAddressLoweringPass() {
  return new MCS51StackAddressLowering();
}
