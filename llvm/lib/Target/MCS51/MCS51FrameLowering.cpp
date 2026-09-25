#include "MCS51FrameLowering.h"
#include "MCS51InstrInfo.h"
#include "MCS51RegisterInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

MCS51FrameLowering::MCS51FrameLowering()
    : TargetFrameLowering(StackGrowsUp, Align(1), 1) {}

void MCS51FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  MachineBasicBlock::iterator I = MBB.begin();
  while (I != MBB.end() &&
         (I->isDebugInstr() || I->getFlag(MachineInstr::FrameSetup)))
    ++I;
  for (uint64_t N = 0; N != MF.getFrameInfo().getStackSize(); ++N)
    BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::INC_DIRECT))
        .addImm(0x81)
        .addReg(MCS51::SP, RegState::ImplicitDefine)
        .setMIFlag(MachineInstr::FrameSetup);
}

void MCS51FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  auto I = MBB.getFirstTerminator();
  for (uint64_t N = 0; N != MF.getFrameInfo().getStackSize(); ++N)
    BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::DEC_DIRECT))
        .addImm(0x81)
        .addReg(MCS51::SP, RegState::ImplicitDefine)
        .setMIFlag(MachineInstr::FrameDestroy);
}

StackOffset MCS51FrameLowering::getFrameIndexReference(
    const MachineFunction &MF, int FI, Register &FrameReg) const {
  FrameReg = MCS51::SP;
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return StackOffset::getFixed(MFI.getObjectOffset(FI) - MFI.getStackSize());
}

bool MCS51FrameLowering::hasFPImpl(const MachineFunction &) const {
  return false;
}
