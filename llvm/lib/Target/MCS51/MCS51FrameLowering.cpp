#include "MCS51FrameLowering.h"

using namespace llvm;

MCS51FrameLowering::MCS51FrameLowering()
    : TargetFrameLowering(StackGrowsDown, Align(1), 0) {}

void MCS51FrameLowering::emitPrologue(MachineFunction &, MachineBasicBlock &) const {}

void MCS51FrameLowering::emitEpilogue(MachineFunction &, MachineBasicBlock &) const {}

bool MCS51FrameLowering::hasFPImpl(const MachineFunction &) const {
  return false;
}
