//===- MCS51TargetTransformInfo.h - MCS-51 TTI ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MCS51_MCS51TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51TARGETTRANSFORMINFO_H

#include "MCS51TargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/Support/MathExtras.h"

namespace llvm {

class MCS51TTIImpl final : public BasicTTIImplBase<MCS51TTIImpl> {
  using BaseT = BasicTTIImplBase<MCS51TTIImpl>;
  friend BaseT;

  const MCS51Subtarget *ST;
  const MCS51TargetLowering *TLI;
  bool MinSize;

  const MCS51Subtarget *getST() const { return ST; }
  const MCS51TargetLowering *getTLI() const { return TLI; }

public:
  explicit MCS51TTIImpl(const MCS51TargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()), MinSize(F.hasMinSize()) {}

  unsigned getInliningThresholdMultiplier() const override {
    // At -Oz, outlined calls are especially cheap compared with duplicated
    // code on MCS-51. Keep only calls LLVM considers unconditionally
    // profitable; other optimization levels retain LLVM's normal heuristic.
    return MinSize ? 0 : 1;
  }

  // The generic cost model charges one unit for an i32 or i64 operation, but
  // the 8-bit MCS-51 expands it into a sequence per byte, and shifts,
  // multiplies and divisions into loops or runtime calls. Scale the costs so
  // that loop unrolling does not replicate wide operations.
  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty, TargetTransformInfo::TargetCostKind CostKind,
      TargetTransformInfo::OperandValueInfo Opd1Info = {TargetTransformInfo::OK_AnyValue, TargetTransformInfo::OP_None},
      TargetTransformInfo::OperandValueInfo Opd2Info = {TargetTransformInfo::OK_AnyValue, TargetTransformInfo::OP_None},
      ArrayRef<const Value *> Args = {},
      const Instruction *CxtI = nullptr) const override {
    InstructionCost Cost = BaseT::getArithmeticInstrCost(
        Opcode, Ty, CostKind, Opd1Info, Opd2Info, Args, CxtI);
    if (!Ty->getScalarType()->isIntegerTy() || Ty->getScalarSizeInBits() <= 8)
      return Cost;
    unsigned Bytes = divideCeil(Ty->getScalarSizeInBits(), 8);
    switch (Opcode) {
    case Instruction::Mul:
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::URem:
    case Instruction::SRem:
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
      return Cost * Bytes * Bytes;
    default:
      return Cost * Bytes;
    }
  }

  InstructionCost getMemoryOpCost(
      unsigned Opcode, Type *Src, Align Alignment, unsigned AddressSpace,
      TargetTransformInfo::TargetCostKind CostKind,
      TargetTransformInfo::OperandValueInfo OpInfo = {TargetTransformInfo::OK_AnyValue, TargetTransformInfo::OP_None},
      const Instruction *I = nullptr) const override {
    InstructionCost Cost = BaseT::getMemoryOpCost(
        Opcode, Src, Alignment, AddressSpace, CostKind, OpInfo, I);
    if (!Src->getScalarType()->isIntegerTy() ||
        Src->getScalarSizeInBits() <= 8)
      return Cost;
    return Cost * divideCeil(Src->getScalarSizeInBits(), 8);
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MCS51_MCS51TARGETTRANSFORMINFO_H
