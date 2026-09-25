#ifndef LLVM_LIB_TARGET_MCS51_MCS51FRAMELOWERING_H
#define LLVM_LIB_TARGET_MCS51_MCS51FRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class MCS51FrameLowering final : public TargetFrameLowering {
public:
  MCS51FrameLowering();
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif
