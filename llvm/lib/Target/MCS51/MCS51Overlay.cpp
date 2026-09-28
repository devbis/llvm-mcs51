//===-- MCS51Overlay.cpp - MCS-51 static frame overlay -------------------===//
//
// This pass moves non-escaping, fixed-size local objects into one module-local
// arena in DATA, IDATA, or XDATA. Functions that can be active at the same
// time receive interfering ranges; other functions may reuse the same bytes.
//
//===----------------------------------------------------------------------===//

#include "MCS51.h"
#include "MCS51TargetMachine.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
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
    cl::desc("overlay safe MCS-51 local objects in static RAM"));

enum class OverlaySpace { Auto, Data, IData, XData };
enum PoolIndex : unsigned { DataPool, IDataPool, XDataPool, NumPools };

static unsigned getAddressSpace(unsigned Pool) {
  switch (Pool) {
  case DataPool:
    return MCS51::Data;
  case IDataPool:
    return MCS51::IData;
  default:
    return MCS51::XData;
  }
}

cl::opt<OverlaySpace> MCS51OverlaySpace(
    "mcs51-overlay-space", cl::Hidden, cl::init(OverlaySpace::Auto),
    cl::desc("address space for the MCS-51 overlay arena"),
    cl::values(clEnumValN(OverlaySpace::Auto, "auto",
                          "choose from access pattern and capacity"),
               clEnumValN(OverlaySpace::Data, "data", "direct DATA"),
               clEnumValN(OverlaySpace::IData, "idata", "indirect IDATA"),
               clEnumValN(OverlaySpace::XData, "xdata", "external XDATA")));
cl::opt<unsigned> MCS51OverlayInternalLimit(
    "mcs51-overlay-internal-limit", cl::Hidden, cl::init(80),
    cl::desc("maximum bytes available to overlay in internal DATA/IDATA"));

struct LocalObject {
  AllocaInst *AI;
  uint64_t Size;
  Align Alignment;
  uint64_t Offset = 0;
  unsigned Pool = IDataPool;
};

struct FunctionFrame {
  Function *F;
  SmallVector<LocalObject, 8> Objects;
  uint64_t Size[NumPools] = {};
  Align Alignment[NumPools] = {Align(1), Align(1), Align(1)};
  uint64_t Offset[NumPools] = {};
  bool Eligible = true;
};

static bool isAddressUse(const User *U, const Value *Pointer) {
  if (isa<DbgVariableIntrinsic>(U) || isa<LifetimeIntrinsic>(U))
    return true;
  if (auto *GEP = dyn_cast<GEPOperator>(U))
    return GEP->getPointerOperand() == Pointer;
  if (auto *LI = dyn_cast<LoadInst>(U))
    return LI->getPointerOperand() == Pointer;
  if (auto *SI = dyn_cast<StoreInst>(U))
    return SI->getPointerOperand() == Pointer;
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
      if (isa<GEPOperator>(U))
        Worklist.push_back(cast<Value>(U));
    }
  }
  return true;
}

static bool isDirectDataObject(AllocaInst &AI) {
  SmallVector<const Value *, 16> Worklist{&AI};
  SmallPtrSet<const Value *, 16> Seen;
  while (!Worklist.empty()) {
    const Value *V = Worklist.pop_back_val();
    if (!Seen.insert(V).second)
      continue;
    for (const User *U : V->users()) {
      if (isa<DbgVariableIntrinsic>(U) || isa<LifetimeIntrinsic>(U))
        continue;
      if (auto *LI = dyn_cast<LoadInst>(U)) {
        if (LI->getPointerOperand() == V)
          continue;
      }
      if (auto *SI = dyn_cast<StoreInst>(U)) {
        if (SI->getPointerOperand() == V)
          continue;
      }
      auto *GEP = dyn_cast<GEPOperator>(U);
      if (!GEP || GEP->getPointerOperand() != V)
        return false;
      // Direct DATA instructions need a link-time absolute operand. Restrict
      // this mode to scalar objects; even a constant GEP is not guaranteed to
      // fold to a GlobalAddress before instruction selection.
      return false;
    }
  }
  return true;
}

static uint64_t getInternalGlobalBytes(const Module &M) {
  const DataLayout &DL = M.getDataLayout();
  uint64_t Bytes = 0;
  for (const GlobalVariable &GV : M.globals()) {
    if (GV.getAddressSpace() != MCS51::Data &&
        GV.getAddressSpace() != MCS51::IData)
      continue;
    // A declaration may be defined in another object and consume the same
    // linker-allocated DATA/IDATA window. In that case the module cannot
    // prove that an internal arena will fit.
    if (GV.isDeclaration())
      return UINT64_MAX;
    TypeSize Size = DL.getTypeAllocSize(GV.getValueType());
    if (Size.isScalable() || Size.getFixedValue() > UINT64_MAX - Bytes)
      return UINT64_MAX;
    Align A = std::max(GV.getAlign().valueOrOne(), DL.getPreferredAlign(&GV));
    Bytes = alignTo(Bytes, A) + Size.getFixedValue();
  }
  return Bytes;
}

static bool replaceAllocaWithArenaSlot(AllocaInst &AI, Value *Arena,
                                       ArrayType *ArenaTy, uint64_t Offset,
                                       unsigned AddressSpace) {
  SmallVector<GetElementPtrInst *, 8> GEPs;
  SmallVector<LifetimeIntrinsic *, 4> Lifetimes;
  SmallVector<Value *, 16> Worklist{&AI};
  SmallPtrSet<Value *, 16> Seen;
  while (!Worklist.empty()) {
    Value *V = Worklist.pop_back_val();
    if (!Seen.insert(V).second)
      continue;
    for (User *U : V->users()) {
      if (auto *Lifetime = dyn_cast<LifetimeIntrinsic>(U)) {
        Lifetimes.push_back(Lifetime);
        continue;
      }
      if (isa<DbgVariableIntrinsic>(U))
        continue;
      if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
        GEPs.push_back(GEP);
        Worklist.push_back(GEP);
        continue;
      }
      if (auto *Load = dyn_cast<LoadInst>(U)) {
        if (Load->getPointerOperand() == V)
          continue;
      }
      if (auto *Store = dyn_cast<StoreInst>(U))
        if (Store->getPointerOperand() == V)
          continue;
      return false;
    }
  }

  LLVMContext &C = AI.getContext();
  IRBuilder<> EntryBuilder(&*AI.getParent()->getFirstInsertionPt());
  Value *ByteBase = EntryBuilder.CreateInBoundsGEP(
      ArenaTy, Arena, {EntryBuilder.getInt32(0), EntryBuilder.getInt64(Offset)},
      "overlay.slot");
  Value *ObjectBase = EntryBuilder.CreateBitCast(
      ByteBase, PointerType::get(C, AddressSpace), AI.getName() + ".overlay");
  DenseMap<Value *, Value *> Remapped;
  Remapped[&AI] = ObjectBase;

  for (GetElementPtrInst *GEP : GEPs) {
    Value *NewBase = Remapped.lookup(GEP->getPointerOperand());
    if (!NewBase)
      return false;
    SmallVector<Value *, 4> Indices;
    for (Value *Index : GEP->indices())
      Indices.push_back(Index);
    IRBuilder<> GEPBuilder(GEP);
    auto *NewGEP = cast<GetElementPtrInst>(
        GEPBuilder.CreateGEP(GEP->getSourceElementType(), NewBase, Indices,
                             GEP->getName() + ".overlay"));
    NewGEP->setIsInBounds(GEP->isInBounds());
    NewGEP->setNoWrapFlags(GEP->getNoWrapFlags());
    NewGEP->setDebugLoc(GEP->getDebugLoc());
    Remapped[GEP] = NewGEP;
  }

  for (Value *V : Seen) {
    Value *NewPointer = Remapped.lookup(V);
    if (!NewPointer)
      continue;
    SmallVector<User *, 8> Users;
    for (User *U : V->users())
      Users.push_back(U);
    for (User *U : Users) {
      if (auto *Dbg = dyn_cast<DbgVariableIntrinsic>(U)) {
        Dbg->replaceVariableLocationOp(V, NewPointer);
      } else if (auto *Load = dyn_cast<LoadInst>(U)) {
        if (Load->getPointerOperand() == V)
          Load->setOperand(Load->getPointerOperandIndex(), NewPointer);
      } else if (auto *Store = dyn_cast<StoreInst>(U)) {
        if (Store->getPointerOperand() == V)
          Store->setOperand(Store->getPointerOperandIndex(), NewPointer);
      }
    }
  }

  for (LifetimeIntrinsic *Lifetime : Lifetimes)
    Lifetime->eraseFromParent();
  for (auto It = GEPs.rbegin(); It != GEPs.rend(); ++It)
    if ((*It)->use_empty())
      (*It)->eraseFromParent();
  if (!AI.use_empty())
    return false;
  AI.eraseFromParent();
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
    const auto *ThinLTOFlag =
        mdconst::dyn_extract_or_null<ConstantInt>(M.getModuleFlag("ThinLTO"));
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
    bool HasInvalidSpaceRequest = false;

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
                       !F.hasFnAttribute("naked") && !Recursive.contains(&F) &&
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
          bool DirectData = isDirectDataObject(*AI);
          unsigned Pool = XDataPool;
          switch (MCS51OverlaySpace) {
          case OverlaySpace::Auto:
            Pool = DirectData ? DataPool : IDataPool;
            break;
          case OverlaySpace::Data:
            if (!DirectData) {
              HasInvalidSpaceRequest = true;
              continue;
            }
            Pool = DataPool;
            break;
          case OverlaySpace::IData:
            Pool = IDataPool;
            break;
          case OverlaySpace::XData:
            Pool = XDataPool;
            break;
          }
          Align A = std::max(AI->getAlign(),
                             DL.getABITypeAlign(AI->getAllocatedType()));
          Frame.Size[Pool] = alignTo(Frame.Size[Pool], A) + Size;
          Frame.Alignment[Pool] = std::max(Frame.Alignment[Pool], A);
          Frame.Objects.push_back({AI, Size, A, Frame.Size[Pool] - Size, Pool});
        }
      }
      if (!Frame.Objects.empty()) {
        FrameIndex[&F] = Frames.size();
        Frames.push_back(std::move(Frame));
      }
    }
    if (HasInvalidSpaceRequest || Frames.empty())
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
        if (Current->isDeclaration() || !Visited.insert(Current).second)
          continue;
        if (auto It = FrameIndex.find(Current);
            It != FrameIndex.end() && It->second != I) {
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

    if (MCS51OverlaySpace == OverlaySpace::Auto) {
      bool NeedsIndirectAccess = false;
      for (FunctionFrame &Frame : Frames)
        for (LocalObject &Object : Frame.Objects)
          NeedsIndirectAccess |= Object.Pool == IDataPool;
      // DATA and IDATA alias in the low internal window. If any object needs
      // indexed access, put direct-only objects into the same IDATA arena so
      // they can share offsets with non-interfering indexed objects.
      if (NeedsIndirectAccess)
        for (FunctionFrame &Frame : Frames)
          for (LocalObject &Object : Frame.Objects)
            if (Object.Pool == DataPool)
              Object.Pool = IDataPool;
    }

    auto RebuildFrameSizes = [&]() {
      for (FunctionFrame &Frame : Frames) {
        for (unsigned Pool = 0; Pool != NumPools; ++Pool) {
          Frame.Size[Pool] = 0;
          Frame.Alignment[Pool] = Align(1);
        }
        for (LocalObject &Object : Frame.Objects) {
          unsigned Pool = Object.Pool;
          Frame.Size[Pool] = alignTo(Frame.Size[Pool], Object.Alignment);
          Object.Offset = Frame.Size[Pool];
          Frame.Size[Pool] += Object.Size;
          Frame.Alignment[Pool] =
              std::max(Frame.Alignment[Pool], Object.Alignment);
        }
      }
    };

    uint64_t ArenaSizes[NumPools] = {};
    Align ArenaAlignments[NumPools] = {Align(1), Align(1), Align(1)};
    auto PlacePool = [&](unsigned Pool) {
      SmallVector<unsigned, 32> Order;
      for (unsigned I = 0; I != N; ++I)
        if (Frames[I].Size[Pool])
          Order.push_back(I);
      BitVector Placed(N);
      llvm::sort(Order, [&](unsigned A, unsigned B) {
        return Frames[A].Size[Pool] > Frames[B].Size[Pool];
      });
      ArenaSizes[Pool] = 0;
      ArenaAlignments[Pool] = Align(1);
      for (unsigned I : Order) {
        FunctionFrame &Frame = Frames[I];
        uint64_t FrameSize = Frame.Size[Pool];
        Align FrameAlignment = Frame.Alignment[Pool];
        if (FrameSize > 65535)
          return false;
        ArenaAlignments[Pool] = std::max(ArenaAlignments[Pool], FrameAlignment);
        uint64_t Candidate = 0;
        while (true) {
          Candidate = alignTo(Candidate, FrameAlignment);
          if (Candidate > 65535 - FrameSize)
            return false;
          bool Conflict = false;
          for (int J = Interferes[I].find_first(); J >= 0;
               J = Interferes[I].find_next(J)) {
            const FunctionFrame &Other = Frames[J];
            if (!Placed.test(J))
              continue;
            if (Candidate < Other.Offset[Pool] + Other.Size[Pool] &&
                Other.Offset[Pool] < Candidate + FrameSize) {
              Candidate = Other.Offset[Pool] + Other.Size[Pool];
              Conflict = true;
              break;
            }
          }
          if (!Conflict)
            break;
        }
        Frame.Offset[Pool] = Candidate;
        Placed.set(I);
        ArenaSizes[Pool] = std::max(ArenaSizes[Pool], Candidate + FrameSize);
      }
      return ArenaSizes[Pool] <= 65535;
    };

    auto PlaceAllPools = [&]() {
      for (unsigned Pool = 0; Pool != NumPools; ++Pool)
        if (!PlacePool(Pool))
          return false;
      return true;
    };

    RebuildFrameSizes();
    if (!PlaceAllPools())
      return false;

    uint64_t InternalBytes = getInternalGlobalBytes(M);
    auto InternalArenaSize = [&]() -> uint64_t {
      if (ArenaSizes[DataPool] > UINT64_MAX - ArenaSizes[IDataPool])
        return UINT64_MAX;
      return ArenaSizes[DataPool] + ArenaSizes[IDataPool];
    };
    auto FitsInternal = [&]() {
      uint64_t ArenaSize = InternalArenaSize();
      return InternalBytes != UINT64_MAX && ArenaSize != UINT64_MAX &&
             InternalBytes <= MCS51OverlayInternalLimit &&
             ArenaSize <= MCS51OverlayInternalLimit - InternalBytes;
    };

    if (MCS51OverlaySpace == OverlaySpace::Auto && !FitsInternal()) {
      // Keep direct-only scalar slots in DATA when possible, and move the
      // indexed objects as a group to XDATA when the shared internal window
      // cannot hold both internal arenas.
      for (FunctionFrame &Frame : Frames)
        for (LocalObject &Object : Frame.Objects)
          if (Object.Pool == IDataPool)
            Object.Pool = XDataPool;
      RebuildFrameSizes();
      if (!PlaceAllPools())
        return false;
      if (!FitsInternal()) {
        for (FunctionFrame &Frame : Frames)
          for (LocalObject &Object : Frame.Objects)
            if (Object.Pool == DataPool)
              Object.Pool = XDataPool;
        RebuildFrameSizes();
        if (!PlaceAllPools())
          return false;
      }
    } else if (MCS51OverlaySpace != OverlaySpace::XData && !FitsInternal()) {
      return false;
    }

    LLVMContext &C = M.getContext();
    Type *I8 = Type::getInt8Ty(C);
    GlobalVariable *Arenas[NumPools] = {};
    ArrayType *ArenaTypes[NumPools] = {};
    for (unsigned Pool = 0; Pool != NumPools; ++Pool) {
      if (!ArenaSizes[Pool])
        continue;
      unsigned AddressSpace = getAddressSpace(Pool);
      ArenaTypes[Pool] = ArrayType::get(I8, ArenaSizes[Pool]);
      StringRef Name = Pool == DataPool    ? "__mcs51_overlay_data"
                       : Pool == IDataPool ? "__mcs51_overlay_idata"
                                           : "__mcs51_overlay_xdata";
      Arenas[Pool] = new GlobalVariable(
          M, ArenaTypes[Pool], false, GlobalValue::InternalLinkage,
          ConstantAggregateZero::get(ArenaTypes[Pool]), Name, nullptr,
          GlobalVariable::NotThreadLocal, AddressSpace);
      if (Pool == XDataPool)
        Arenas[Pool]->setSection(".bss.mcs51.overlay");
      Arenas[Pool]->setAlignment(ArenaAlignments[Pool]);
    }

    for (FunctionFrame &Frame : Frames) {
      for (LocalObject &Object : Frame.Objects) {
        unsigned Pool = Object.Pool;
        if (!replaceAllocaWithArenaSlot(
                *Object.AI, Arenas[Pool], ArenaTypes[Pool],
                Frame.Offset[Pool] + Object.Offset, getAddressSpace(Pool)))
          report_fatal_error("unsupported MCS-51 overlay pointer use");
      }
    }
    return true;
  }
};

char MCS51Overlay::ID = 0;
} // namespace

ModulePass *llvm::createMCS51OverlayPass() { return new MCS51Overlay(); }
