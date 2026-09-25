#ifndef LLVM_LIB_TARGET_MCS51_MCS51ISELLOWERING_H
#define LLVM_LIB_TARGET_MCS51_MCS51ISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MCS51Subtarget;

class MCS51TargetLowering final : public TargetLowering {
public:
  MCS51TargetLowering(const TargetMachine &TM,
                      const MCS51Subtarget &STI);

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals,
                      const SDLoc &DL, SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif
