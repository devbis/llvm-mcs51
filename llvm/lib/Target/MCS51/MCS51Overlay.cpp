//===-- MCS51Overlay.cpp - MCS-51 static frame overlay -------------------===//
//
// This pass moves non-escaping, fixed-size local objects into one module-local
// XDATA arena. Functions that can be active at the same time receive
// interfering ranges; other functions may reuse the same bytes.
//
//===----------------------------------------------------------------------===//

#include "MCS51TargetMachine.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

namespace {
cl::opt<bool> EnableMCS51Overlay(
    "mcs51-overlay", cl::Hidden, cl::init(false),
    cl::desc("overlay safe MCS-51 local objects in static XDATA"));

struct LocalObject {
  AllocaInst *AI;
  uint64_t Size;
  Align Alignment;
  uint64_t Offset = 0;
};

struct FunctionFrame {
  Function *F;
  SmallVector<LocalObject, 8> Objects;
  uint64_t Size = 0;
  Align Alignment = Align(1);
  uint64_t Offset = 0;
  bool Eligible = true;
};

static bool isAddressUse(const User *U, const Value *Pointer) {
  if (isa<DbgInfoIntrinsic>(U) || isa<LifetimeIntrinsic>(U))
    return true;
  if (auto *GEP = dyn_cast<GEPOperator>(U))
    return GEP->getPointerOperand() == Pointer;
  if (auto *BC = dyn_cast<BitCastOperator>(U))
    return BC->getOperand(0) == Pointer;
  if (auto *ASC = dyn_cast<AddrSpaceCastOperator>(U))
    return ASC->getPointerOperand() == Pointer;
  if (auto *LI = dyn_cast<LoadInst>(U))
    return LI->getPointerOperand() == Pointer;
  if (auto *SI = dyn_cast<StoreInst>(U))
    return SI->getPointerOperand() == Pointer;
  if (auto *PN = dyn_cast<PHINode>(U))
    return llvm::is_contained(PN->incoming_values(), Pointer);
  if (auto *SI = dyn_cast<SelectInst>(U))
    return SI->getTrueValue() == Pointer || SI->getFalseValue() == Pointer;
  if (auto *MI = dyn_cast<MemIntrinsic>(U))
    return MI->getRawDest() == Pointer ||
           (isa<MemTransferInst>(MI) &&
            cast<MemTransferInst>(MI)->getRawSource() == Pointer);
  if (auto *Cmp = dyn_cast<ICmpInst>(U))
    return Cmp->getOperand(0) == Pointer || Cmp->getOperand(1) == Pointer;
  return false;
}

static bool isNonEscaping(AllocaInst &AI) {
  SmallVector<const Value *, 16> Worklist{&AI};
  SmallPtrSet<const Value *, 16> Seen;
  while (!Worklist.empty()) {
    const Value *V = Worklist.pop_back_val();
    if (!Seen.insert(V).second)
      continue;
    for (const User *U : V->users()) {
      if (!isAddressUse(U, V))
        return false;
      if (isa<GEPOperator>(U) || isa<BitCastOperator>(U) ||
          isa<AddrSpaceCastOperator>(U) || isa<PHINode>(U) ||
          isa<SelectInst>(U))
        Worklist.push_back(cast<Value>(U));
    }
  }
  return true;
}

static bool isDirectCallTo(const CallBase &CB, const Function *F) {
  return CB.getCalledOperand()->stripPointerCasts() == F;
}

class MCS51Overlay final : public ModulePass {
public:
  static char ID;
  MCS51Overlay() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    if (!EnableMCS51Overlay)
      return false;

    const DataLayout &DL = M.getDataLayout();
    const auto *ThinLTOFlag = mdconst::dyn_extract_or_null<ConstantInt>(
        M.getModuleFlag("ThinLTO"));
    const bool IsFullLTO = ThinLTOFlag && ThinLTOFlag->isZero();
    SmallPtrSet<Function *, 16> Recursive;
    for (Function &Root : M) {
      if (Root.isDeclaration())
        continue;
      SmallVector<Function *, 16> Worklist;
      SmallPtrSet<Function *, 16> Visited;
      for (BasicBlock &BB : Root)
        for (Instruction &I : BB)
          if (auto *CB = dyn_cast<CallBase>(&I))
            if (Function *Callee = CB->getCalledFunction())
              Worklist.push_back(Callee);
      while (!Worklist.empty()) {
        Function *Current = Worklist.pop_back_val();
        if (Current == &Root) {
          Recursive.insert(&Root);
          break;
        }
        if (Current->isDeclaration() || !Visited.insert(Current).second)
          continue;
        for (BasicBlock &BB : *Current)
          for (Instruction &I : BB)
            if (auto *CB = dyn_cast<CallBase>(&I))
              if (Function *Callee = CB->getCalledFunction())
                Worklist.push_back(Callee);
      }
    }

    SmallPtrSet<Function *, 16> InterruptReachable;
    for (Function &Root : M) {
      if (Root.isDeclaration() || !Root.hasFnAttribute("interrupt"))
        continue;
      SmallVector<Function *, 16> Worklist{&Root};
      while (!Worklist.empty()) {
        Function *Current = Worklist.pop_back_val();
        if (Current->isDeclaration() ||
            !InterruptReachable.insert(Current).second)
          continue;
        for (BasicBlock &BB : *Current)
          for (Instruction &I : BB)
            if (auto *CB = dyn_cast<CallBase>(&I))
              if (Function *Callee = CB->getCalledFunction())
                Worklist.push_back(Callee);
      }
    }

    SmallVector<FunctionFrame, 32> Frames;
    DenseMap<const Function *, unsigned> FrameIndex;
    bool HasIndirectCall = false;

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      FunctionFrame Frame;
      Frame.F = &F;
      // Only local functions can be proven absent from callbacks originating
      // outside this module. Interrupts and address-taken functions are
      // handled by their own storage and are never overlaid.
      Frame.Eligible = (F.hasLocalLinkage() || IsFullLTO) &&
                       (F.hasLocalLinkage() || F.isDSOLocal()) &&
                       !F.hasFnAttribute("interrupt") &&
                       !F.hasFnAttribute("naked") &&
                       !Recursive.contains(&F) &&
                       !InterruptReachable.contains(&F);
      for (User *U : F.users()) {
        auto *CB = dyn_cast<CallBase>(U);
        if (!CB || !isDirectCallTo(*CB, &F))
          Frame.Eligible = false;
      }
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (auto *CB = dyn_cast<CallBase>(&I))
            HasIndirectCall |= !isa<InlineAsm>(CB->getCalledOperand()) &&
                               !CB->getCalledFunction();
          auto *AI = dyn_cast<AllocaInst>(&I);
          if (!AI)
            continue;
          // An alloca in a loop or conditional block may execute more than
          // once and create simultaneously live instances.
          if (AI->getParent() != &F.getEntryBlock())
            continue;
          auto *Count = dyn_cast<ConstantInt>(AI->getArraySize());
          TypeSize TypeBytes = DL.getTypeAllocSize(AI->getAllocatedType());
          if (!Frame.Eligible || !Count || TypeBytes.isScalable() ||
              AI->isUsedWithInAlloca() || AI->isSwiftError() ||
              !isNonEscaping(*AI))
            continue;
          uint64_t CountValue = Count->getZExtValue();
          uint64_t ElementSize = TypeBytes.getFixedValue();
          if (CountValue && ElementSize > UINT64_MAX / CountValue)
            continue;
          uint64_t Size = CountValue * ElementSize;
          if (Size == 0)
            continue;
          Align A = std::max(AI->getAlign(), DL.getABITypeAlign(AI->getAllocatedType()));
          Frame.Size = alignTo(Frame.Size, A) + Size;
          Frame.Alignment = std::max(Frame.Alignment, A);
          Frame.Objects.push_back({AI, Size, A, Frame.Size - Size});
        }
      }
      if (!Frame.Objects.empty()) {
        FrameIndex[&F] = Frames.size();
        Frames.push_back(std::move(Frame));
      }
    }
    if (Frames.empty())
      return false;

    const unsigned N = Frames.size();
    SmallVector<BitVector, 32> Interferes;
    for (unsigned I = 0; I != N; ++I)
      Interferes.emplace_back(N);
    for (unsigned I = 0; I != N; ++I) {
      if (HasIndirectCall) {
        for (unsigned J = 0; J != N; ++J)
          if (I != J)
            Interferes[I].set(J);
        continue;
      }
      SmallVector<Function *, 16> Worklist;
      SmallPtrSet<Function *, 16> Visited;
      for (BasicBlock &BB : *Frames[I].F)
        for (Instruction &Inst : BB)
          if (auto *CB = dyn_cast<CallBase>(&Inst))
            if (Function *Callee = CB->getCalledFunction())
              Worklist.push_back(Callee);
      while (!Worklist.empty()) {
        Function *Current = Worklist.pop_back_val();
        if (Current->isDeclaration() ||
            !Visited.insert(Current).second)
          continue;
        if (auto It = FrameIndex.find(Current); It != FrameIndex.end() &&
            It->second != I) {
          Interferes[I].set(It->second);
          Interferes[It->second].set(I);
        }
        for (BasicBlock &BB : *Current)
          for (Instruction &Inst : BB)
            if (auto *CB = dyn_cast<CallBase>(&Inst))
              if (Function *Callee = CB->getCalledFunction())
                Worklist.push_back(Callee);
      }
    }

    SmallVector<unsigned, 32> Order;
    for (unsigned I = 0; I != N; ++I)
      Order.push_back(I);
    llvm::sort(Order, [&](unsigned A, unsigned B) {
      return Frames[A].Size > Frames[B].Size;
    });
    uint64_t ArenaSize = 0;
    Align ArenaAlignment(1);
    for (unsigned I : Order) {
      FunctionFrame &Frame = Frames[I];
      if (Frame.Size > 65535)
        return false;
      ArenaAlignment = std::max(ArenaAlignment, Frame.Alignment);
      uint64_t Candidate = 0;
      while (true) {
        Candidate = alignTo(Candidate, Frame.Alignment);
        if (Candidate > 65535 - Frame.Size)
          return false;
        bool Conflict = false;
        for (int J = Interferes[I].find_first(); J >= 0;
             J = Interferes[I].find_next(J)) {
          const FunctionFrame &Other = Frames[J];
          if (Candidate < Other.Offset + Other.Size &&
              Other.Offset < Candidate + Frame.Size) {
            Candidate = Other.Offset + Other.Size;
            Conflict = true;
            break;
          }
        }
        if (!Conflict)
          break;
      }
      Frame.Offset = Candidate;
      ArenaSize = std::max(ArenaSize, Candidate + Frame.Size);
    }
    // The target's near XDATA pointer and address space are 16 bits.
    if (ArenaSize > 65535)
      return false;

    LLVMContext &C = M.getContext();
    Type *I8 = Type::getInt8Ty(C);
    ArrayType *ArenaTy = ArrayType::get(I8, ArenaSize);
    auto *Arena = new GlobalVariable(
        M, ArenaTy, false, GlobalValue::InternalLinkage,
        ConstantAggregateZero::get(ArenaTy), "__mcs51_overlay",
        nullptr, GlobalVariable::NotThreadLocal, 0);
    Arena->setSection(".bss.mcs51.overlay");
    Arena->setAlignment(ArenaAlignment);

    for (FunctionFrame &Frame : Frames) {
      IRBuilder<> EntryBuilder(&*Frame.F->getEntryBlock().getFirstInsertionPt());
      for (LocalObject &Object : Frame.Objects) {
        Value *Base = EntryBuilder.CreateInBoundsGEP(
            ArenaTy, Arena, {EntryBuilder.getInt32(0),
                             EntryBuilder.getInt64(Frame.Offset + Object.Offset)},
            "overlay.slot");
        Value *Typed = EntryBuilder.CreateBitCast(Base, Object.AI->getType());
        Object.AI->replaceAllUsesWith(Typed);
        Object.AI->eraseFromParent();
      }
    }
    return true;
  }
};

char MCS51Overlay::ID = 0;
} // namespace

ModulePass *llvm::createMCS51OverlayPass() { return new MCS51Overlay(); }
