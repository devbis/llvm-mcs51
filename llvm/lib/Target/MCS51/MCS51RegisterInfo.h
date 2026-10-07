#ifndef LLVM_LIB_TARGET_MCS51_MCS51REGISTERINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51REGISTERINFO_H

#include "MCS51FrameLowering.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "MCS51GenRegisterInfo.inc"

namespace llvm {

class MCS51RegisterInfo final : public MCS51GenRegisterInfo {
public:
  MCS51RegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const override;
  BitVector getReservedRegs(const MachineFunction &MF) const override;
  bool eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;
  /// Stack arguments are pushed one by one inside the call sequence. Only the
  /// forward walk accumulates MCS51InstrInfo::getSPAdjust per instruction.
  bool eliminateFrameIndicesBackwards() const override { return false; }
  Register getFrameRegister(const MachineFunction &MF) const override;
  const TargetRegisterClass *getPointerRegClass(unsigned Kind = 0) const override;
};

} // namespace llvm

#endif
