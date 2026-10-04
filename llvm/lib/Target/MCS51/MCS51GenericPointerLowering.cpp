//===-- MCS51GenericPointerLowering.cpp - generic pointer calls ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCS51.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include <string>
#include "llvm/Support/Casting.h"

using namespace llvm;

namespace {
class MCS51GenericPointerLowering final : public FunctionPass {
public:
  static char ID;
  MCS51GenericPointerLowering() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    bool Changed = false;
    SmallVector<MemCpyInst *, 4> DynamicCopies;
    SmallVector<MemMoveInst *, 4> DynamicMoves;
    for (Instruction &I : instructions(F)) {
      if (auto *Copy = dyn_cast<MemCpyInst>(&I)) {
        if (!isa<ConstantInt>(Copy->getLength()))
          DynamicCopies.push_back(Copy);
      } else if (auto *Move = dyn_cast<MemMoveInst>(&I)) {
        if (!isa<ConstantInt>(Move->getLength()))
          DynamicMoves.push_back(Move);
      }
    }
    for (MemCpyInst *Copy : DynamicCopies) {
      lowerDynamicAddressSpaceCopy(*Copy);
      Changed = true;
    }
    for (MemMoveInst *Move : DynamicMoves) {
      lowerDynamicAddressSpaceMove(*Move);
      Changed = true;
    }

    SmallVector<GetElementPtrInst *, 8> GenericGEPs;
    for (Instruction &I : instructions(F))
      if (auto *GEP = dyn_cast<GetElementPtrInst>(&I))
        if (GEP->getPointerAddressSpace() == MCS51::Generic)
          GenericGEPs.push_back(GEP);
    for (GetElementPtrInst *GEP : GenericGEPs) {
      lowerGenericGEP(*GEP);
      Changed = true;
    }

    for (Instruction &I : instructions(F)) {
      Value *Pointer = nullptr;
      if (auto *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getPointerAddressSpace() == MCS51::Generic)
          Pointer = LI->getPointerOperand();
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getPointerAddressSpace() == MCS51::Generic)
          Pointer = SI->getPointerOperand();
      }
      auto *Cast = dyn_cast_or_null<AddrSpaceCastOperator>(Pointer);
      if (!Cast || Cast->getDestAddressSpace() != MCS51::Generic ||
          Cast->getSrcAddressSpace() == MCS51::Generic ||
          !isGenericTagSupported(Cast->getSrcAddressSpace()))
        continue;
      if (Cast->getSrcAddressSpace() == MCS51::Default &&
          hasMergedPointerProvenance(Cast->getPointerOperand()))
        continue;
      if (isa<StoreInst>(&I) && Cast->getSrcAddressSpace() == MCS51::Code)
        continue;
      if (auto *LI = dyn_cast<LoadInst>(&I))
        LI->setOperand(0, Cast->getPointerOperand());
      else
        cast<StoreInst>(I).setOperand(1, Cast->getPointerOperand());
      Changed = true;
    }

    for (Instruction &I : instructions(F)) {
      for (Use &Operand : I.operands()) {
        auto *Cast = dyn_cast<AddrSpaceCastOperator>(Operand.get());
        if (!Cast || (Cast->getSrcAddressSpace() != MCS51::Generic &&
                      Cast->getDestAddressSpace() != MCS51::Generic))
          continue;
        IRBuilder<> B(&I);
        DenseMap<const PHINode *, PHINode *> GenericPointerPhis;
        Operand.set(lowerAddressSpaceCast(
            B, Cast->getPointerOperand(), Cast->getType(),
            Cast->getSrcAddressSpace(), Cast->getDestAddressSpace(),
            GenericPointerPhis));
        Changed = true;
      }
    }

    SmallVector<Instruction *, 16> Worklist;
    for (Instruction &I : instructions(F)) {
      if (auto *Cast = dyn_cast<AddrSpaceCastInst>(&I)) {
        if (Cast->use_empty()) {
          Worklist.push_back(Cast);
        } else if (Cast->getSrcAddressSpace() == MCS51::Generic ||
                   Cast->getDestAddressSpace() == MCS51::Generic) {
          Worklist.push_back(Cast);
        }
      } else if (auto *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getPointerAddressSpace() == MCS51::Generic)
          Worklist.push_back(LI);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getPointerAddressSpace() == MCS51::Generic)
          Worklist.push_back(SI);
      }
    }

    for (Instruction *I : Worklist) {
      if (auto *Cast = dyn_cast<AddrSpaceCastInst>(I)) {
        if (Cast->use_empty())
          Cast->eraseFromParent();
        else
          lowerAddressSpaceCast(*Cast);
        Changed = true;
      } else if (auto *LI = dyn_cast<LoadInst>(I)) {
        lowerLoad(*LI);
        Changed = true;
      } else {
        lowerStore(*cast<StoreInst>(I));
        Changed = true;
      }
    }
    return Changed;
  }

  StringRef getPassName() const override {
    return "MCS-51 generic pointer lowering";
  }

private:
  static void lowerGenericGEP(GetElementPtrInst &GEP) {
    IRBuilder<> B(&GEP);
    Module &M = *GEP.getModule();
    LLVMContext &C = M.getContext();
    const DataLayout &DL = M.getDataLayout();
    Type *I16 = Type::getInt16Ty(C);
    Type *I32 = Type::getInt32Ty(C);
    Value *Offset = nullptr;

    for (gep_type_iterator GTI = gep_type_begin(&GEP), E = gep_type_end(&GEP);
         GTI != E; ++GTI) {
      Value *Index = GTI.getOperand();
      if (StructType *STy = GTI.getStructTypeOrNull()) {
        auto *Field = cast<ConstantInt>(Index);
        uint64_t FieldOffset =
            DL.getStructLayout(STy)->getElementOffset(Field->getZExtValue());
        Value *FieldOffsetValue = B.getInt16(FieldOffset);
        Offset = Offset
                     ? B.CreateAdd(Offset, FieldOffsetValue, "gptr.gep.offset")
                     : FieldOffsetValue;
        continue;
      }

      uint64_t Stride = GTI.getSequentialElementStride(DL);
      Value *Index16 = B.CreateSExtOrTrunc(Index, I16, "gptr.gep.index");
      Value *ScaledIndex = Index16;
      if (Stride != 1)
        ScaledIndex =
            B.CreateMul(Index16, B.getInt16(Stride), "gptr.gep.scaled.index");
      Offset = Offset ? B.CreateAdd(Offset, ScaledIndex, "gptr.gep.offset")
                      : ScaledIndex;
    }

    Value *BaseBits =
        B.CreatePtrToInt(GEP.getPointerOperand(), I32, "gptr.gep.base");
    Value *BaseAddress = B.CreateTrunc(BaseBits, I16, "gptr.gep.address");
    Value *Address = Offset
                         ? B.CreateAdd(BaseAddress, Offset, "gptr.gep.address")
                         : BaseAddress;
    Value *TagAndPadding =
        B.CreateAnd(BaseBits, B.getInt32(0xffff0000), "gptr.gep.tag");
    Value *AddressBits = B.CreateZExt(Address, I32, "gptr.gep.address.bits");
    Value *ResultBits = B.CreateOr(TagAndPadding, AddressBits, "gptr.gep.bits");
    Value *Result = B.CreateIntToPtr(ResultBits, GEP.getType(), "gptr.gep");
    GEP.replaceAllUsesWith(Result);
    GEP.eraseFromParent();
  }

  static bool isGenericTagSupported(unsigned AddressSpace) {
    switch (AddressSpace) {
    case MCS51::Default:
    case MCS51::Data:
    case MCS51::IData:
    case MCS51::PData:
    case MCS51::XData:
    case MCS51::Code:
      return true;
    default:
      return false;
    }
  }

  static bool pointsToStackObject(Value *Pointer) {
    SmallPtrSet<Value *, 8> Seen;
    while (Pointer && Seen.insert(Pointer).second) {
      if (isa<AllocaInst>(Pointer))
        return true;
      if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
        Pointer = GEP->getPointerOperand();
        continue;
      }
      if (auto *Cast = dyn_cast<BitCastOperator>(Pointer)) {
        Pointer = Cast->getOperand(0);
        continue;
      }
      if (auto *Cast = dyn_cast<AddrSpaceCastOperator>(Pointer)) {
        Pointer = Cast->getPointerOperand();
        continue;
      }
      return false;
    }
    return false;
  }

  static Value *getMemIntrinsicMemoryPointer(Value *Pointer) {
    while (true) {
      if (auto *Cast = dyn_cast<AddrSpaceCastOperator>(Pointer)) {
        // Clang casts qualified pointers to the default pointer type when it
        // builds llvm.memcpy. The cast is only an intrinsic ABI adaptation;
        // retain the address space used by the actual memory operation.
        if (Cast->getDestAddressSpace() == MCS51::Default) {
          Pointer = Cast->getPointerOperand();
          continue;
        }
      }
      if (auto *Cast = dyn_cast<BitCastOperator>(Pointer)) {
        Pointer = Cast->getOperand(0);
        continue;
      }
      return Pointer;
    }
  }

  static void lowerDynamicAddressSpaceCopy(MemCpyInst &Copy) {
    Function &F = *Copy.getFunction();
    LLVMContext &C = F.getContext();
    Type *I8Ty = Type::getInt8Ty(C);
    Value *Dst = getMemIntrinsicMemoryPointer(Copy.getRawDest());
    Value *Src = getMemIntrinsicMemoryPointer(Copy.getRawSource());
    unsigned DstAS = cast<PointerType>(Dst->getType())->getAddressSpace();
    unsigned SrcAS = cast<PointerType>(Src->getType())->getAddressSpace();
    bool DstIsStack = DstAS == MCS51::Default && pointsToStackObject(Dst);
    bool SrcIsStack = SrcAS == MCS51::Default && pointsToStackObject(Src);
    const DataLayout &DL = F.getParent()->getDataLayout();
    APInt DstOffset(DL.getIndexTypeSizeInBits(Dst->getType()), 0);
    APInt SrcOffset(DL.getIndexTypeSizeInBits(Src->getType()), 0);
    if (DstIsStack)
      Dst = Dst->stripAndAccumulateConstantOffsets(DL, DstOffset, true);
    if (SrcIsStack)
      Src = Src->stripAndAccumulateConstantOffsets(DL, SrcOffset, true);
    if (DstAS == MCS51::Code)
      report_fatal_error("cannot copy bytes into MCS-51 code memory");
    if (DstAS == MCS51::Bit || SrcAS == MCS51::Bit)
      report_fatal_error("byte copy through MCS-51 bit pointers is unsupported");

    BasicBlock *Preheader = Copy.getParent();
    BasicBlock *Continue =
        Preheader->splitBasicBlock(Copy.getIterator(), "mcs51.memcpy.cont");
    Preheader->getTerminator()->eraseFromParent();
    BasicBlock *Loop = BasicBlock::Create(C, "mcs51.memcpy.loop", &F, Continue);
    BasicBlock *Body = BasicBlock::Create(C, "mcs51.memcpy.body", &F, Continue);

    IRBuilder<> Entry(Preheader);
    Entry.SetCurrentDebugLocation(Copy.getDebugLoc());
    Entry.CreateBr(Loop);

    IRBuilder<> LoopBuilder(Loop);
    LoopBuilder.SetCurrentDebugLocation(Copy.getDebugLoc());
    Type *LengthTy = Copy.getLength()->getType();
    PHINode *Index = LoopBuilder.CreatePHI(LengthTy, 2, "mcs51.memcpy.index");
    Index->addIncoming(ConstantInt::get(LengthTy, 0), Preheader);
    Value *More = LoopBuilder.CreateICmpULT(Index, Copy.getLength(),
                                             "mcs51.memcpy.more");
    LoopBuilder.CreateCondBr(More, Body, Continue);

    IRBuilder<> BodyBuilder(Body);
    BodyBuilder.SetCurrentDebugLocation(Copy.getDebugLoc());
    Value *SourceIndex = Index;
    if (SrcIsStack) {
      // Stack objects live in IDATA, whose address is an 8-bit @Ri pointer.
      // Keep the GEP index in that width instead of asking instruction
      // selection to truncate a DPTR value after it has been selected.
      SourceIndex = BodyBuilder.CreateIntCast(Index, I8Ty, false,
                                              "mcs51.memcpy.src.index8");
    }
    if (SrcIsStack && !SrcOffset.isZero()) {
      Value *Offset = ConstantInt::get(
          I8Ty, static_cast<uint8_t>(SrcOffset.getSExtValue()));
      SourceIndex = BodyBuilder.CreateAdd(SourceIndex, Offset,
                                          "mcs51.memcpy.src.index");
    } else if (!SrcIsStack && !SrcOffset.isZero()) {
      Value *Offset = ConstantInt::get(LengthTy, SrcOffset.getSExtValue());
      SourceIndex = BodyBuilder.CreateAdd(Index, Offset,
                                          "mcs51.memcpy.src.index");
    }
    Value *DestinationIndex = Index;
    if (DstIsStack) {
      DestinationIndex = BodyBuilder.CreateIntCast(
          Index, I8Ty, false, "mcs51.memcpy.dst.index8");
    }
    if (DstIsStack && !DstOffset.isZero()) {
      Value *Offset = ConstantInt::get(
          I8Ty, static_cast<uint8_t>(DstOffset.getSExtValue()));
      DestinationIndex = BodyBuilder.CreateAdd(
          DestinationIndex, Offset, "mcs51.memcpy.dst.index");
    } else if (!DstIsStack && !DstOffset.isZero()) {
      Value *Offset = ConstantInt::get(LengthTy, DstOffset.getSExtValue());
      DestinationIndex = BodyBuilder.CreateAdd(
          Index, Offset, "mcs51.memcpy.dst.index");
    }
    Value *SourceAddress =
        BodyBuilder.CreateGEP(I8Ty, Src, SourceIndex, "mcs51.memcpy.src");
    Value *DestinationAddress = BodyBuilder.CreateGEP(
        I8Ty, Dst, DestinationIndex, "mcs51.memcpy.dst");
    if (SrcIsStack)
      SourceAddress = BodyBuilder.CreateAddrSpaceCast(
          SourceAddress, PointerType::get(C, MCS51::IData),
          "mcs51.memcpy.src.idata");
    if (DstIsStack)
      DestinationAddress = BodyBuilder.CreateAddrSpaceCast(
          DestinationAddress, PointerType::get(C, MCS51::IData),
          "mcs51.memcpy.dst.idata");
    LoadInst *Byte = BodyBuilder.CreateLoad(I8Ty, SourceAddress);
    Byte->setVolatile(Copy.isVolatile());
    StoreInst *Store = BodyBuilder.CreateStore(Byte, DestinationAddress);
    Store->setVolatile(Copy.isVolatile());
    Value *Next = BodyBuilder.CreateAdd(Index, ConstantInt::get(LengthTy, 1),
                                        "mcs51.memcpy.next");
    BodyBuilder.CreateBr(Loop);
    Index->addIncoming(Next, Body);

    Copy.eraseFromParent();
  }

  static void lowerDynamicAddressSpaceMove(MemMoveInst &Move) {
    Function &F = *Move.getFunction();
    LLVMContext &C = F.getContext();
    const DataLayout &DL = F.getParent()->getDataLayout();
    Type *I8Ty = Type::getInt8Ty(C);
    Type *I16Ty = Type::getInt16Ty(C);
    Type *I32Ty = Type::getInt32Ty(C);
    Value *Dst = getMemIntrinsicMemoryPointer(Move.getRawDest());
    Value *Src = getMemIntrinsicMemoryPointer(Move.getRawSource());
    unsigned DstAS = cast<PointerType>(Dst->getType())->getAddressSpace();
    unsigned SrcAS = cast<PointerType>(Src->getType())->getAddressSpace();
    bool DstIsStack = DstAS == MCS51::Default && pointsToStackObject(Dst);
    bool SrcIsStack = SrcAS == MCS51::Default && pointsToStackObject(Src);
    APInt DstOffset(DL.getIndexTypeSizeInBits(Dst->getType()), 0);
    APInt SrcOffset(DL.getIndexTypeSizeInBits(Src->getType()), 0);
    Value *DstStackObject = nullptr;
    Value *SrcStackObject = nullptr;
    if (DstIsStack)
      DstStackObject = Dst =
          Dst->stripAndAccumulateConstantOffsets(DL, DstOffset, true);
    if (SrcIsStack)
      SrcStackObject = Src =
          Src->stripAndAccumulateConstantOffsets(DL, SrcOffset, true);
    if (DstAS == MCS51::Code)
      report_fatal_error("cannot move bytes into MCS-51 code memory");
    if (DstAS == MCS51::Bit || SrcAS == MCS51::Bit)
      report_fatal_error("byte move through MCS-51 bit pointers is unsupported");

    BasicBlock *Preheader = Move.getParent();
    BasicBlock *Continue =
        Preheader->splitBasicBlock(Move.getIterator(), "mcs51.memmove.cont");
    Preheader->getTerminator()->eraseFromParent();
    BasicBlock *Direction =
        BasicBlock::Create(C, "mcs51.memmove.direction", &F, Continue);
    BasicBlock *ForwardLoop =
        BasicBlock::Create(C, "mcs51.memmove.forward", &F, Continue);
    BasicBlock *ForwardBody =
        BasicBlock::Create(C, "mcs51.memmove.forward.body", &F, Continue);
    BasicBlock *BackwardSetup =
        BasicBlock::Create(C, "mcs51.memmove.backward.setup", &F, Continue);
    BasicBlock *BackwardInit =
        BasicBlock::Create(C, "mcs51.memmove.backward.init", &F, Continue);
    BasicBlock *BackwardLoop =
        BasicBlock::Create(C, "mcs51.memmove.backward", &F, Continue);
    BasicBlock *BackwardBody =
        BasicBlock::Create(C, "mcs51.memmove.backward.body", &F, Continue);

    IRBuilder<> Entry(Preheader);
    Entry.SetCurrentDebugLocation(Move.getDebugLoc());
    Entry.CreateBr(Direction);

    IRBuilder<> DirectionBuilder(Direction);
    DirectionBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    Value *OverlapAfterSource = ConstantInt::getFalse(C);
    if (DstIsStack && SrcIsStack) {
      if (DstStackObject == SrcStackObject && DstOffset.sgt(SrcOffset)) {
        APInt Distance = DstOffset - SrcOffset;
        OverlapAfterSource = DirectionBuilder.CreateICmpUGT(
            Move.getLength(),
            ConstantInt::get(Move.getLength()->getType(),
                             Distance.getZExtValue()),
            "mcs51.memmove.stack.overlap");
      }
    } else {
      auto PhysicalAddressSpace = [](unsigned AS) -> unsigned {
        return AS == MCS51::Default ? MCS51::XData : AS;
      };
      unsigned DstPhysicalAS =
          DstIsStack ? MCS51::IData : PhysicalAddressSpace(DstAS);
      unsigned SrcPhysicalAS =
          SrcIsStack ? MCS51::IData : PhysicalAddressSpace(SrcAS);
      bool SameInternalRAM =
          (DstPhysicalAS == MCS51::Data && SrcPhysicalAS == MCS51::IData) ||
          (DstPhysicalAS == MCS51::IData && SrcPhysicalAS == MCS51::Data);
      bool HasGeneric = DstAS == MCS51::Generic || SrcAS == MCS51::Generic;
      bool Comparable = HasGeneric
                            ? (DstAS == MCS51::Generic &&
                               SrcAS == MCS51::Generic) ||
                                  (DstAS == MCS51::Generic &&
                                   isGenericTagSupported(SrcAS)) ||
                                  (SrcAS == MCS51::Generic &&
                                   isGenericTagSupported(DstAS))
                            : DstPhysicalAS == SrcPhysicalAS || SameInternalRAM;
      if (Comparable) {
        auto AddressAsI16 = [&](Value *Pointer, bool IsStack,
                                const APInt &Offset) -> Value * {
          if (IsStack) {
            Pointer = DirectionBuilder.CreateAddrSpaceCast(
                Pointer, PointerType::get(C, MCS51::IData),
                "mcs51.memmove.stack.base");
            if (!Offset.isZero())
              Pointer = DirectionBuilder.CreateGEP(
                  I8Ty, Pointer,
                  ConstantInt::get(
                      I8Ty, static_cast<uint8_t>(Offset.getSExtValue())),
                  "mcs51.memmove.stack.address");
          }
          unsigned AS = cast<PointerType>(Pointer->getType())->getAddressSpace();
          unsigned Width = DL.getPointerSizeInBits(AS);
          Type *IntTy = IntegerType::get(C, Width);
          Value *Bits = DirectionBuilder.CreatePtrToInt(Pointer, IntTy);
          return DirectionBuilder.CreateZExtOrTrunc(Bits, I16Ty);
        };
        Value *DstAddress = AddressAsI16(Dst, DstIsStack, DstOffset);
        Value *SrcAddress = AddressAsI16(Src, SrcIsStack, SrcOffset);
        Value *DstAfterSrc = DirectionBuilder.CreateICmpUGT(
            DstAddress, SrcAddress, "mcs51.memmove.dst.after.src");
        Value *Distance = DirectionBuilder.CreateSub(
            DstAddress, SrcAddress, "mcs51.memmove.distance");
        Value *InRange = DirectionBuilder.CreateICmpULT(
            Distance, Move.getLength(), "mcs51.memmove.in.range");
        OverlapAfterSource = DirectionBuilder.CreateAnd(
            DstAfterSrc, InRange, "mcs51.memmove.overlap");
        if (HasGeneric) {
          auto GetTag = [&](Value *Pointer, unsigned AS) -> Value * {
            if (AS != MCS51::Generic)
              return DirectionBuilder.getInt8(getGenericTag(AS, Pointer));
            Value *Bits = DirectionBuilder.CreatePtrToInt(Pointer, I32Ty);
            Value *Tag = DirectionBuilder.CreateLShr(
                Bits, DirectionBuilder.getInt32(16));
            return DirectionBuilder.CreateTrunc(Tag, I8Ty);
          };
          Value *SameTag = DirectionBuilder.CreateICmpEQ(
              GetTag(Dst, DstAS), GetTag(Src, SrcAS),
              "mcs51.memmove.same.tag");
          OverlapAfterSource = DirectionBuilder.CreateAnd(
              OverlapAfterSource, SameTag, "mcs51.memmove.same.space");
        }
      }
    }
    DirectionBuilder.CreateCondBr(OverlapAfterSource, BackwardSetup,
                                  ForwardLoop);

    Type *LengthTy = Move.getLength()->getType();
    auto CopyByteAt = [&](IRBuilder<> &Builder, Value *Index,
                          const Twine &Suffix) {
      auto MakeIndex = [&](bool IsStack, const APInt &Offset,
                           const Twine &Name) -> Value * {
        Value *Result = IsStack
                            ? Builder.CreateIntCast(Index, I8Ty, false,
                                                    Name + ".index8")
                            : Index;
        if (IsStack && !Offset.isZero())
          Result = Builder.CreateAdd(
              Result,
              ConstantInt::get(I8Ty,
                               static_cast<uint8_t>(Offset.getSExtValue())),
              Name + ".offset");
        else if (!IsStack && !Offset.isZero())
          Result = Builder.CreateAdd(
              Result, ConstantInt::get(LengthTy, Offset.getSExtValue()),
              Name + ".offset");
        return Result;
      };
      Value *SourceIndex = MakeIndex(SrcIsStack, SrcOffset,
                                     Twine("mcs51.memmove.src") + Suffix);
      Value *DestinationIndex = MakeIndex(
          DstIsStack, DstOffset, Twine("mcs51.memmove.dst") + Suffix);
      Value *SourceBase = Src;
      Value *DestinationBase = Dst;
      if (SrcIsStack)
        SourceBase = Builder.CreateAddrSpaceCast(
            SourceBase, PointerType::get(C, MCS51::IData),
            "mcs51.memmove.src.idata.base");
      if (DstIsStack)
        DestinationBase = Builder.CreateAddrSpaceCast(
            DestinationBase, PointerType::get(C, MCS51::IData),
            "mcs51.memmove.dst.idata.base");
      Value *SourceAddress =
          Builder.CreateGEP(I8Ty, SourceBase, SourceIndex,
                            "mcs51.memmove.src.ptr");
      Value *DestinationAddress = Builder.CreateGEP(
          I8Ty, DestinationBase, DestinationIndex,
          "mcs51.memmove.dst.ptr");
      if (SrcAS == MCS51::Data) {
        Value *Bits = Builder.CreatePtrToInt(SourceAddress, I8Ty);
        SourceAddress = Builder.CreateIntToPtr(
            Bits, PointerType::get(C, MCS51::IData));
      }
      if (DstAS == MCS51::Data) {
        Value *Bits = Builder.CreatePtrToInt(DestinationAddress, I8Ty);
        DestinationAddress = Builder.CreateIntToPtr(
            Bits, PointerType::get(C, MCS51::IData));
      }
      LoadInst *Byte = Builder.CreateLoad(I8Ty, SourceAddress);
      Byte->setVolatile(Move.isVolatile());
      StoreInst *Store = Builder.CreateStore(Byte, DestinationAddress);
      Store->setVolatile(Move.isVolatile());
    };

    IRBuilder<> ForwardBuilder(ForwardLoop);
    ForwardBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    PHINode *ForwardIndex =
        ForwardBuilder.CreatePHI(LengthTy, 2, "mcs51.memmove.index");
    ForwardIndex->addIncoming(ConstantInt::get(LengthTy, 0), Direction);
    Value *ForwardMore = ForwardBuilder.CreateICmpULT(
        ForwardIndex, Move.getLength(), "mcs51.memmove.forward.more");
    ForwardBuilder.CreateCondBr(ForwardMore, ForwardBody, Continue);

    IRBuilder<> ForwardBodyBuilder(ForwardBody);
    ForwardBodyBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    CopyByteAt(ForwardBodyBuilder, ForwardIndex, ".forward");
    Value *NextForward = ForwardBodyBuilder.CreateAdd(
        ForwardIndex, ConstantInt::get(LengthTy, 1),
        "mcs51.memmove.next.forward");
    ForwardBodyBuilder.CreateBr(ForwardLoop);
    ForwardIndex->addIncoming(NextForward, ForwardBody);

    IRBuilder<> BackwardSetupBuilder(BackwardSetup);
    BackwardSetupBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    Value *HasBytes = BackwardSetupBuilder.CreateICmpNE(
        Move.getLength(), ConstantInt::get(LengthTy, 0),
        "mcs51.memmove.backward.has.bytes");
    BackwardSetupBuilder.CreateCondBr(HasBytes, BackwardInit, Continue);

    IRBuilder<> BackwardInitBuilder(BackwardInit);
    BackwardInitBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    Value *LastIndex = BackwardInitBuilder.CreateSub(
        Move.getLength(), ConstantInt::get(LengthTy, 1),
        "mcs51.memmove.last.index");
    BackwardInitBuilder.CreateBr(BackwardLoop);

    IRBuilder<> BackwardBuilder(BackwardLoop);
    BackwardBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    PHINode *BackwardIndex =
        BackwardBuilder.CreatePHI(LengthTy, 2, "mcs51.memmove.index");
    BackwardIndex->addIncoming(LastIndex, BackwardInit);
    BackwardBuilder.CreateBr(BackwardBody);

    IRBuilder<> BackwardBodyBuilder(BackwardBody);
    BackwardBodyBuilder.SetCurrentDebugLocation(Move.getDebugLoc());
    CopyByteAt(BackwardBodyBuilder, BackwardIndex, ".backward");
    Value *AtFirst = BackwardBodyBuilder.CreateICmpEQ(
        BackwardIndex, ConstantInt::get(LengthTy, 0),
        "mcs51.memmove.backward.done");
    Value *NextBackward = BackwardBodyBuilder.CreateSub(
        BackwardIndex, ConstantInt::get(LengthTy, 1),
        "mcs51.memmove.next.backward");
    BackwardBodyBuilder.CreateCondBr(AtFirst, Continue, BackwardLoop);
    BackwardIndex->addIncoming(NextBackward, BackwardBody);

    Move.eraseFromParent();
  }

  static bool hasMergedPointerProvenance(Value *Pointer) {
    SmallPtrSet<Value *, 8> Seen;
    while (Pointer && Seen.insert(Pointer).second) {
      if (isa<SelectInst>(Pointer) || isa<PHINode>(Pointer))
        return true;
      if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
        Pointer = GEP->getPointerOperand();
        continue;
      }
      if (auto *Cast = dyn_cast<BitCastOperator>(Pointer)) {
        Pointer = Cast->getOperand(0);
        continue;
      }
      if (auto *Cast = dyn_cast<AddrSpaceCastOperator>(Pointer)) {
        Pointer = Cast->getPointerOperand();
        continue;
      }
      return false;
    }
    return false;
  }

  static uint8_t getGenericTag(unsigned AddressSpace, Value *Pointer) {
    // Match the classic MCS-51 generic pointer encoding used by the runtime:
    // CODE=0x80, DATA/IDATA=0x40, PDATA=0x60, XDATA=0x00. The default near
    // pointer uses XDATA, while addresses rooted in local allocas use IDATA.
    switch (AddressSpace) {
    case MCS51::Default:
      return pointsToStackObject(Pointer) ? 0x40 : 0x00;
    case MCS51::XData:
      return 0x00;
    case MCS51::Data:
    case MCS51::IData:
      return 0x40;
    case MCS51::PData:
      return 0x60;
    case MCS51::Code:
      return 0x80;
    default:
      report_fatal_error(
          "unsupported MCS-51 address space in generic pointer cast");
    }
  }

  static Value *lowerAddressSpaceCast(
      IRBuilder<> &B, Value *Pointer, Type *DestTy, unsigned SrcAS,
      unsigned DstAS,
      DenseMap<const PHINode *, PHINode *> &GenericPointerPhis) {
    Module &M = *B.GetInsertBlock()->getModule();
    LLVMContext &C = M.getContext();
    if (DstAS == MCS51::Generic) {
      if (auto *Select = dyn_cast<SelectInst>(Pointer)) {
        Value *TruePointer =
            lowerAddressSpaceCast(B, Select->getTrueValue(), DestTy, SrcAS,
                                  DstAS, GenericPointerPhis);
        Value *FalsePointer =
            lowerAddressSpaceCast(B, Select->getFalseValue(), DestTy, SrcAS,
                                  DstAS, GenericPointerPhis);
        return B.CreateSelect(Select->getCondition(), TruePointer, FalsePointer,
                              "gptr.cast.select");
      }
      if (auto *PointerPhi = dyn_cast<PHINode>(Pointer)) {
        auto Existing = GenericPointerPhis.find(PointerPhi);
        if (Existing != GenericPointerPhis.end())
          return Existing->second;

        BasicBlock *BB = PointerPhi->getParent();
        PHINode *GenericPhi =
            PHINode::Create(DestTy, PointerPhi->getNumIncomingValues(),
                            "gptr.cast.phi", BB->getFirstNonPHIIt());
        GenericPointerPhis[PointerPhi] = GenericPhi;
        for (unsigned I = 0; I != PointerPhi->getNumIncomingValues(); ++I) {
          BasicBlock *IncomingBB = PointerPhi->getIncomingBlock(I);
          IRBuilder<> IncomingBuilder(IncomingBB->getTerminator());
          Value *IncomingPointer = lowerAddressSpaceCast(
              IncomingBuilder, PointerPhi->getIncomingValue(I), DestTy, SrcAS,
              DstAS, GenericPointerPhis);
          GenericPhi->addIncoming(IncomingPointer, IncomingBB);
        }
        return GenericPhi;
      }
    }

    Type *I16 = Type::getInt16Ty(C);
    Value *Address;
    if (DstAS == MCS51::Generic && SrcAS == MCS51::Default &&
        pointsToStackObject(Pointer)) {
      // Preserve the frame-index-to-IDATA lowering so the backend can form
      // the stack object's run-time address relative to SP.
      Type *IDataPtrTy = PointerType::get(C, MCS51::IData);
      Value *IDataPointer =
          B.CreateAddrSpaceCast(Pointer, IDataPtrTy, "gptr.cast.idata.pointer");
      Value *IDataAddress = B.CreatePtrToInt(IDataPointer, Type::getInt8Ty(C),
                                             "gptr.cast.idata.address");
      Address = B.CreateZExt(IDataAddress, I16, "gptr.cast.address");
    } else {
      Address = B.CreatePtrToInt(Pointer, I16, "gptr.cast.address");
    }
    Value *Result;
    if (DstAS == MCS51::Generic) {
      Type *I32 = Type::getInt32Ty(C);
      Value *Bits = B.CreateZExt(Address, I32, "gptr.cast.bits");
      uint8_t Tag = getGenericTag(SrcAS, Pointer);
      if (Tag) {
        Value *TagBits = B.CreateShl(B.getInt32(Tag), 16, "gptr.cast.tag");
        Bits = B.CreateOr(Bits, TagBits, "gptr.cast.encoded");
      }
      Result = B.CreateIntToPtr(Bits, DestTy, "gptr.cast");
    } else {
      const DataLayout &DL = M.getDataLayout();
      unsigned PointerBits = DL.getPointerSizeInBits(DstAS);
      Type *IntTy = IntegerType::get(C, PointerBits);
      Value *Truncated = PointerBits == 16
                             ? Address
                             : B.CreateTrunc(Address, IntTy, "gptr.cast.trunc");
      Result = B.CreateIntToPtr(Truncated, DestTy, "gptr.cast");
    }
    Result->setName("gptr.cast");
    return Result;
  }

  static void lowerAddressSpaceCast(AddrSpaceCastInst &Cast) {
    IRBuilder<> B(&Cast);
    DenseMap<const PHINode *, PHINode *> GenericPointerPhis;
    Value *Result = lowerAddressSpaceCast(
        B, Cast.getOperand(0), Cast.getType(), Cast.getSrcAddressSpace(),
        Cast.getDestAddressSpace(), GenericPointerPhis);
    if (auto *I = dyn_cast<Instruction>(Result))
      I->setDebugLoc(Cast.getDebugLoc());
    Cast.replaceAllUsesWith(Result);
    Cast.eraseFromParent();
  }

  static SmallVector<Value *, 4> getPointerParts(IRBuilder<> &B,
                                                 Value *Pointer,
                                                 uint16_t ByteOffset = 0) {
    LLVMContext &C = B.getContext();
    Type *I8 = Type::getInt8Ty(C);
    Type *I32 = Type::getInt32Ty(C);
    Value *Bits = B.CreatePtrToInt(Pointer, I32, "gptr.bits");
    if (ByteOffset) {
      Type *I16 = Type::getInt16Ty(C);
      Value *Address = B.CreateTrunc(Bits, I16, "gptr.byte.address");
      Address = B.CreateAdd(Address, B.getInt16(ByteOffset),
                            "gptr.byte.address");
      Value *Tag = B.CreateAnd(Bits, B.getInt32(0xffff0000), "gptr.byte.tag");
      Bits = B.CreateOr(Tag, B.CreateZExt(Address, I32), "gptr.byte.bits");
    }
    SmallVector<Value *, 4> Parts;
    for (unsigned Offset : {0u, 8u, 16u, 24u}) {
      Value *Part = Bits;
      if (Offset)
        Part = B.CreateLShr(Bits, Offset, "gptr.part.shift");
      Parts.push_back(B.CreateTrunc(Part, I8, "gptr.part"));
    }
    return Parts;
  }

  static uint64_t getAggregateSize(const DataLayout &DL, Type *Ty) {
    TypeSize Size = DL.getTypeAllocSize(Ty);
    if (Size.isScalable())
      report_fatal_error("scalable MCS-51 generic aggregate access");
    return Size.getFixedValue();
  }

  static Type *getGenericAccessType(Type *Ty, const DataLayout &DL,
                                    LLVMContext &C) {
    if (Ty->isPointerTy())
      return IntegerType::get(C, DL.getPointerSizeInBits(
                                     cast<PointerType>(Ty)->getAddressSpace()));
    return getMemoryType(Ty, C);
  }

  static unsigned getLargestByteChunk(uint64_t Remaining) {
    for (unsigned Size : {8u, 4u, 2u, 1u})
      if (Remaining >= Size)
        return Size;
    llvm_unreachable("empty MCS-51 aggregate byte chunk");
  }

  static Value *lowerGenericScalarLoadValue(IRBuilder<> &B, Module &M,
                                            Value *Pointer, Type *Ty,
                                            uint64_t Offset,
                                            bool IsVolatile) {
    LLVMContext &C = M.getContext();
    const DataLayout &DL = M.getDataLayout();
    Type *AccessTy = getGenericAccessType(Ty, DL, C);
    std::string Name =
        (Twine("__mcs51_gptrget") + getSuffix(AccessTy)).str();
    SmallVector<Value *, 4> Args =
        getPointerParts(B, Pointer, static_cast<uint16_t>(Offset));
    Type *CallTy = AccessTy->isIntegerTy(8) ? Type::getInt16Ty(C) : AccessTy;
    CallInst *Call = createCall(B, M, Name, CallTy, Args);
    if (IsVolatile)
      Call->setConvergent();
    else
      Call->setOnlyReadsMemory();
    Value *Result = Call;
    if (CallTy != AccessTy)
      Result = B.CreateTrunc(Result, AccessTy, "gptr.aggregate.narrow");
    if (Ty->isPointerTy())
      Result = B.CreateIntToPtr(Result, Ty, "gptr.aggregate.pointer");
    else if (Ty->isIntegerTy(1))
      Result = B.CreateTrunc(Result, Ty, "gptr.aggregate.bool");
    return Result;
  }

  static void lowerGenericScalarStoreValue(IRBuilder<> &B, Module &M,
                                           Value *Pointer, Value *ValueToStore,
                                           Type *Ty, uint64_t Offset,
                                           bool IsVolatile) {
    LLVMContext &C = M.getContext();
    const DataLayout &DL = M.getDataLayout();
    Type *AccessTy = getGenericAccessType(Ty, DL, C);
    Value *Stored = ValueToStore;
    Type *StorageTy = AccessTy;
    if (Ty->isPointerTy())
      Stored = B.CreatePtrToInt(Stored, AccessTy,
                                "gptr.aggregate.pointer.bits");
    else if (Ty->isIntegerTy(1))
      Stored = B.CreateZExt(Stored, AccessTy, "gptr.aggregate.bool");
    else if (Ty->isFloatTy()) {
      StorageTy = Type::getInt32Ty(C);
      Stored = B.CreateBitCast(Stored, StorageTy,
                               "gptr.aggregate.float.bits");
    }

    SmallVector<Value *, 12> Args =
        getPointerParts(B, Pointer, static_cast<uint16_t>(Offset));
    unsigned NumBytes = DL.getTypeStoreSize(StorageTy).getFixedValue();
    Type *I8Ty = Type::getInt8Ty(C);
    for (unsigned I = 0; I < NumBytes; ++I) {
      Value *Part = Stored;
      if (I)
        Part = B.CreateLShr(Stored, I * 8, "gptr.aggregate.value.shift");
      Args.push_back(B.CreateTrunc(Part, I8Ty, "gptr.aggregate.value.byte"));
    }
    std::string Name =
        (Twine("__mcs51_gptrput") + getSuffix(AccessTy)).str();
    CallInst *Call = createCall(B, M, Name, Type::getVoidTy(C), Args);
    if (IsVolatile)
      Call->setConvergent();
  }

  static Value *lowerAggregateLoadValue(IRBuilder<> &B, Module &M,
                                        Value *Pointer, Type *Ty,
                                        uint64_t Offset, bool IsVolatile) {
    LLVMContext &C = M.getContext();
    const DataLayout &DL = M.getDataLayout();
    if (auto *STy = dyn_cast<StructType>(Ty)) {
      Value *Result = UndefValue::get(Ty);
      const StructLayout *Layout = DL.getStructLayout(STy);
      for (unsigned I = 0; I < STy->getNumElements(); ++I) {
        Type *FieldTy = STy->getElementType(I);
        if (!getAggregateSize(DL, FieldTy))
          continue;
        Value *Field = lowerAggregateLoadValue(
            B, M, Pointer, FieldTy, Offset + Layout->getElementOffset(I),
            IsVolatile);
        Result = B.CreateInsertValue(Result, Field, I, "gptr.aggregate.field");
      }
      return Result;
    }
    if (auto *ATy = dyn_cast<ArrayType>(Ty)) {
      Value *Result = UndefValue::get(Ty);
      Type *ElementTy = ATy->getElementType();
      if (ElementTy->isIntegerTy(8)) {
        uint64_t I = 0;
        while (I < ATy->getNumElements()) {
          unsigned ChunkBytes =
              getLargestByteChunk(ATy->getNumElements() - I);
          Type *ChunkTy = IntegerType::get(C, ChunkBytes * 8);
          Value *Chunk = lowerGenericScalarLoadValue(
              B, M, Pointer, ChunkTy, Offset + I, IsVolatile);
          for (unsigned J = 0; J < ChunkBytes; ++J) {
            Value *Byte = Chunk;
            if (J)
              Byte = B.CreateLShr(Chunk, J * 8,
                                  "gptr.aggregate.byte.shift");
            if (ChunkTy != ElementTy)
              Byte = B.CreateTrunc(Byte, ElementTy, "gptr.aggregate.byte");
            Result = B.CreateInsertValue(Result, Byte, I + J,
                                         "gptr.aggregate.element");
          }
          I += ChunkBytes;
        }
        return Result;
      }
      uint64_t Stride = getAggregateSize(DL, ElementTy);
      for (uint64_t I = 0; I < ATy->getNumElements(); ++I) {
        Value *Element = lowerAggregateLoadValue(
            B, M, Pointer, ElementTy, Offset + I * Stride, IsVolatile);
        Result = B.CreateInsertValue(Result, Element, I,
                                     "gptr.aggregate.element");
      }
      return Result;
    }
    return lowerGenericScalarLoadValue(B, M, Pointer, Ty, Offset,
                                       IsVolatile);
  }

  static void lowerAggregateStoreValue(IRBuilder<> &B, Module &M,
                                       Value *Pointer, Value *ValueToStore,
                                       Type *Ty, uint64_t Offset,
                                       bool IsVolatile) {
    LLVMContext &C = M.getContext();
    const DataLayout &DL = M.getDataLayout();
    if (auto *STy = dyn_cast<StructType>(Ty)) {
      const StructLayout *Layout = DL.getStructLayout(STy);
      for (unsigned I = 0; I < STy->getNumElements(); ++I) {
        Type *FieldTy = STy->getElementType(I);
        if (!getAggregateSize(DL, FieldTy))
          continue;
        Value *Field = B.CreateExtractValue(ValueToStore, I,
                                            "gptr.aggregate.field");
        lowerAggregateStoreValue(
            B, M, Pointer, Field, FieldTy,
            Offset + Layout->getElementOffset(I), IsVolatile);
      }
      return;
    }
    if (auto *ATy = dyn_cast<ArrayType>(Ty)) {
      Type *ElementTy = ATy->getElementType();
      if (ElementTy->isIntegerTy(8)) {
        uint64_t I = 0;
        while (I < ATy->getNumElements()) {
          unsigned ChunkBytes =
              getLargestByteChunk(ATy->getNumElements() - I);
          Type *ChunkTy = IntegerType::get(C, ChunkBytes * 8);
          Value *Chunk = ConstantInt::get(ChunkTy, 0);
          for (unsigned J = 0; J < ChunkBytes; ++J) {
            Value *Byte = B.CreateExtractValue(ValueToStore, I + J,
                                               "gptr.aggregate.byte");
            Value *Part = Byte;
            if (ChunkTy != ElementTy)
              Part = B.CreateZExt(Byte, ChunkTy,
                                  "gptr.aggregate.byte.extend");
            if (J)
              Part = B.CreateShl(Part, J * 8,
                                 "gptr.aggregate.byte.shift");
            Chunk = B.CreateOr(Chunk, Part, "gptr.aggregate.chunk");
          }
          lowerGenericScalarStoreValue(B, M, Pointer, Chunk, ChunkTy,
                                       Offset + I, IsVolatile);
          I += ChunkBytes;
        }
        return;
      }
      uint64_t Stride = getAggregateSize(DL, ElementTy);
      for (uint64_t I = 0; I < ATy->getNumElements(); ++I) {
        Value *Element = B.CreateExtractValue(ValueToStore, I,
                                              "gptr.aggregate.element");
        lowerAggregateStoreValue(B, M, Pointer, Element, ElementTy,
                                 Offset + I * Stride, IsVolatile);
      }
      return;
    }
    lowerGenericScalarStoreValue(B, M, Pointer, ValueToStore, Ty, Offset,
                                 IsVolatile);
  }

  static void lowerAggregateLoad(LoadInst &LI) {
    IRBuilder<> B(&LI);
    Value *Result = lowerAggregateLoadValue(
        B, *LI.getModule(), LI.getPointerOperand(), LI.getType(), 0,
        LI.isVolatile());
    if (auto *I = dyn_cast<Instruction>(Result))
      I->setDebugLoc(LI.getDebugLoc());
    LI.replaceAllUsesWith(Result);
    LI.eraseFromParent();
  }

  static void lowerAggregateStore(StoreInst &SI) {
    IRBuilder<> B(&SI);
    lowerAggregateStoreValue(B, *SI.getModule(), SI.getPointerOperand(),
                             SI.getValueOperand(),
                             SI.getValueOperand()->getType(), 0,
                             SI.isVolatile());
    SI.eraseFromParent();
  }

  static Type *getMemoryType(Type *Ty, LLVMContext &C) {
    return Ty->isIntegerTy(1) ? Type::getInt8Ty(C) : Ty;
  }

  static StringRef getSuffix(Type *Ty) {
    if (Ty->isIntegerTy(1) || Ty->isIntegerTy(8))
      return "8";
    if (Ty->isIntegerTy(16))
      return "16";
    if (Ty->isIntegerTy(32))
      return "32";
    if (Ty->isIntegerTy(64))
      return "64";
    if (Ty->isFloatTy())
      return "f32";
    report_fatal_error("unsupported MCS-51 generic pointer access type");
  }

  static CallInst *createCall(IRBuilder<> &B, Module &M, StringRef Name,
                              Type *RetTy, ArrayRef<Value *> Args) {
    SmallVector<Type *, 12> ArgTys;
    for (Value *Arg : Args)
      ArgTys.push_back(Arg->getType());
    FunctionCallee Callee =
        M.getOrInsertFunction(Name, FunctionType::get(RetTy, ArgTys, false));
    CallInst *Call = B.CreateCall(Callee, Args);
    Call->setCallingConv(CallingConv::C);
    return Call;
  }

  static void lowerLoad(LoadInst &LI) {
    if (LI.getType()->isAggregateType()) {
      lowerAggregateLoad(LI);
      return;
    }
    IRBuilder<> B(&LI);
    Module &M = *LI.getModule();
    LLVMContext &C = M.getContext();
    Type *MemTy = getMemoryType(LI.getType(), C);
    SmallVector<Value *, 4> Args = getPointerParts(B, LI.getPointerOperand());
    std::string Name =
        (Twine("__mcs51_gptrget") + getSuffix(LI.getType())).str();
    Type *CallTy = MemTy;
    if (getSuffix(LI.getType()) == "8")
      CallTy = Type::getInt16Ty(C);
    CallInst *Call = createCall(B, M, Name, CallTy, Args);
    if (LI.isVolatile())
      Call->setConvergent();
    else
      Call->setOnlyReadsMemory();
    Call->setDebugLoc(LI.getDebugLoc());
    Value *Result = Call;
    if (CallTy != LI.getType())
      Result = B.CreateTrunc(Result, LI.getType(), "gptr.bool");
    LI.replaceAllUsesWith(Result);
    LI.eraseFromParent();
  }

  static void lowerStore(StoreInst &SI) {
    if (SI.getValueOperand()->getType()->isAggregateType()) {
      lowerAggregateStore(SI);
      return;
    }
    IRBuilder<> B(&SI);
    Module &M = *SI.getModule();
    LLVMContext &C = M.getContext();
    Type *I8 = Type::getInt8Ty(C);
    Type *ValueTy = SI.getValueOperand()->getType();
    Type *MemTy = getMemoryType(ValueTy, C);
    Value *Stored = SI.getValueOperand();
    if (ValueTy->isIntegerTy(1))
      Stored = B.CreateZExt(Stored, I8, "gptr.bool");
    else if (ValueTy->isFloatTy())
      Stored = B.CreateBitCast(Stored, Type::getInt32Ty(C), "gptr.float.bits");

    SmallVector<Value *, 12> Args = getPointerParts(B, SI.getPointerOperand());
    unsigned NumBytes = MemTy->getPrimitiveSizeInBits().getFixedValue() / 8;
    for (unsigned I = 0; I < NumBytes; ++I) {
      Value *Part = Stored;
      if (I)
        Part = B.CreateLShr(Stored, I * 8, "gptr.value.shift");
      Args.push_back(B.CreateTrunc(Part, I8, "gptr.value.byte"));
    }
    std::string Name = (Twine("__mcs51_gptrput") + getSuffix(ValueTy)).str();
    CallInst *Call = createCall(B, M, Name, Type::getVoidTy(C), Args);
    if (SI.isVolatile())
      Call->setConvergent();
    Call->setDebugLoc(SI.getDebugLoc());
    SI.eraseFromParent();
  }
};
} // namespace

char MCS51GenericPointerLowering::ID = 0;

namespace llvm {
FunctionPass *createMCS51GenericPointerLoweringPass() {
  return new MCS51GenericPointerLowering();
}
} // namespace llvm
