#include "MCS51FrameLowering.h"
#include "MCS51InstrInfo.h"
#include "MCS51RegisterInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

MCS51FrameLowering::MCS51FrameLowering()
    : TargetFrameLowering(StackGrowsUp, Align(1), 1) {}

static void emitStackAdjustment(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const TargetInstrInfo &TII, uint64_t Amount,
                                bool Deallocate) {
  if (Amount > 255)
    report_fatal_error(
        "MCS-51 stack frame exceeds 255-byte stack address space");

  // Keep small frames as straight-line INC/DEC instructions. A fixed SFR
  // update avoids code growth proportional to larger frames.
  if (Amount <= (Deallocate ? 5 : 3)) {
    for (uint64_t N = 0; N != Amount; ++N)
      BuildMI(MBB, I, DebugLoc(), TII.get(Deallocate ? MCS51::DEC_DIRECT
                                                     : MCS51::INC_DIRECT))
          .addImm(0x81)
          .addReg(MCS51::SP, RegState::ImplicitDefine)
          .setMIFlag(Deallocate ? MachineInstr::FrameDestroy
                                : MachineInstr::FrameSetup);
    return;
  }

  MachineInstr::MIFlag FrameFlag = Deallocate ? MachineInstr::FrameDestroy
                                              : MachineInstr::FrameSetup;
  BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::MOV_A_SP), MCS51::A)
      .setMIFlag(FrameFlag);
  BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::ADD_A_IMM), MCS51::A)
      .addImm(Deallocate ? static_cast<int64_t>((256 - Amount) & 0xff)
                         : static_cast<int64_t>(Amount))
      .setMIFlag(FrameFlag);
  BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::MOV_SP_A)).setMIFlag(FrameFlag);
  if (Deallocate) {
    // A may hold the function's return value. After releasing the frame, a
    // balanced push/pop preserves A without disturbing the caller's stack.
    // FrameDestroy describes the net adjustment even though the balanced
    // push/pop leaves SP unchanged.
    BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::PUSH_DIRECT))
        .addImm(0xe0)
        .addReg(MCS51::SP, RegState::ImplicitDefine)
        .addReg(MCS51::SP, RegState::Implicit)
        .setMIFlag(MachineInstr::FrameDestroy);
    BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::POP_DIRECT))
        .addImm(0xe0)
        .addReg(MCS51::SP, RegState::ImplicitDefine)
        .addReg(MCS51::SP, RegState::Implicit)
        .setMIFlag(MachineInstr::FrameDestroy);
  }
}

void MCS51FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  bool IsInterrupt = MF.getFunction().hasFnAttribute("interrupt");
  MachineBasicBlock::iterator I = MBB.begin();
  while (I != MBB.end() &&
         (I->isDebugInstr() || I->getFlag(MachineInstr::FrameSetup)))
    ++I;
  if (IsInterrupt) {
    // The interrupted code may have live values in any 8051 register. Save
    // bank 0 plus the accumulator, B, DPTR and PSW before using them in the
    // handler. The 8051 hardware already saved the return PC.
    for (unsigned Address : {0xd0, 0xe0, 0xf0, 0x82, 0x83,
                             0x00, 0x01, 0x02, 0x03,
                             0x04, 0x05, 0x06, 0x07})
      BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::PUSH_DIRECT))
          .addImm(Address)
          .addReg(MCS51::SP, RegState::ImplicitDefine)
          .setMIFlag(MachineInstr::FrameSetup);
  }
  emitStackAdjustment(MBB, I, TII, StackSize, /*Deallocate=*/false);
}

void MCS51FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  auto I = MBB.getFirstTerminator();
  emitStackAdjustment(MBB, I, TII, StackSize, /*Deallocate=*/true);
  if (MF.getFunction().hasFnAttribute("interrupt")) {
    for (unsigned Address : {0x07, 0x06, 0x05, 0x04,
                             0x03, 0x02, 0x01, 0x00,
                             0x83, 0x82, 0xf0, 0xe0, 0xd0})
      BuildMI(MBB, I, DebugLoc(), TII.get(MCS51::POP_DIRECT))
          .addImm(Address)
          .addReg(MCS51::SP, RegState::ImplicitDefine)
          .setMIFlag(MachineInstr::FrameDestroy);
  }
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
