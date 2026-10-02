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
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MCS51_MCS51TARGETTRANSFORMINFO_H
