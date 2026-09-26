#ifndef LLVM_LIB_TARGET_MCS51_MCS51ISELLOWERING_H
#define LLVM_LIB_TARGET_MCS51_MCS51ISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MCS51Subtarget;
class MachineBasicBlock;
class MachineInstr;

class MCS51TargetLowering final : public TargetLowering {
public:
  MCS51TargetLowering(const TargetMachine &TM,
                      const MCS51Subtarget &STI);

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  MVT getRegisterTypeForCallingConv(LLVMContext &Context,
                                    CallingConv::ID CC,
                                    EVT VT) const override;
  unsigned getNumRegistersForCallingConv(LLVMContext &Context,
                                        CallingConv::ID CC,
                                        EVT VT) const override;

  ShiftLegalizationStrategy
  preferredShiftLegalizationStrategy(SelectionDAG &DAG, SDNode *N,
                                     unsigned ExpansionFactor) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals,
                      const SDLoc &DL, SelectionDAG &DAG) const override;

  MachineBasicBlock *EmitInstrWithCustomInserter(
      MachineInstr &MI, MachineBasicBlock *MBB) const override;

private:
  const MCS51Subtarget &STI;
};

} // namespace llvm

#endif
