#include "MCS51RegisterInfo.h"
#include "MCS51.h"
#include "MCS51InstrInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "MCS51GenRegisterInfo.inc"

MCS51RegisterInfo::MCS51RegisterInfo() : MCS51GenRegisterInfo(MCS51::PC) {}

const MCPhysReg *
MCS51RegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

const uint32_t *MCS51RegisterInfo::getCallPreservedMask(
    const MachineFunction &, CallingConv::ID) const {
  return nullptr;
}

BitVector MCS51RegisterInfo::getReservedRegs(const MachineFunction &) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(MCS51::PC);
  Reserved.set(MCS51::SP);
  Reserved.set(MCS51::PSW);
  // R1 is reserved as the indirect pointer for stack frame spill accesses.
  Reserved.set(MCS51::R1);
  return Reserved;
}

bool MCS51RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *) const {
  assert(SPAdj == 0 && "unexpected MCS-51 stack pointer adjustment");
  MachineBasicBlock &MBB = *MI->getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const DebugLoc &DL = MI->getDebugLoc();
  int FI = MI->getOperand(FIOperandNum).getIndex();
  Register FrameReg;
  int64_t Offset = MF.getSubtarget().getFrameLowering()
                       ->getFrameIndexReference(MF, FI, FrameReg)
                       .getFixed();
  Offset += MI->getOperand(FIOperandNum + 1).getImm();
  if (Offset < -128 || Offset > 127)
    report_fatal_error("MCS-51 stack frame exceeds 128-byte displacement");

  auto I = MI->getIterator();
  auto EmitAddress = [&]() {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A).addImm(0x81);
    BuildMI(MBB, I, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A).addImm(Offset);
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(MCS51::R1, RegState::Define);
  };

  if (MI->getOpcode() == MCS51::SPILL_LOAD8 ||
      MI->getOpcode() == MCS51::LOAD_FRAME8 ||
      MI->getOpcode() == MCS51::SPILL_LOAD16) {
    Register Dst = MI->getOperand(0).getReg();
    EmitAddress();
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R1);
    if (MI->getOpcode() == MCS51::SPILL_LOAD8 ||
        MI->getOpcode() == MCS51::LOAD_FRAME8) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), Dst);
    } else {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_RN)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_A));
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A))
          .addReg(MCS51::R1, RegState::Define);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    }
  } else if (MI->getOpcode() == MCS51::SPILL_STORE8 ||
             MI->getOpcode() == MCS51::STORE_FRAME8 ||
             MI->getOpcode() == MCS51::SPILL_STORE16) {
    Register Src = MI->getOperand(FIOperandNum + 2).getReg();
    EmitAddress();
    if (MI->getOpcode() == MCS51::SPILL_STORE8 ||
        MI->getOpcode() == MCS51::STORE_FRAME8) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    } else {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x82);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_RN)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_A));
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A))
          .addReg(MCS51::R1, RegState::Define);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x83);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    }
  } else {
    llvm_unreachable("unexpected MCS-51 frame-index instruction");
  }
  MI->eraseFromParent();
  return true;
}

Register MCS51RegisterInfo::getFrameRegister(const MachineFunction &) const {
  return MCS51::SP;
}

const TargetRegisterClass *
MCS51RegisterInfo::getPointerRegClass(unsigned) const {
  return &MCS51::MCS51PTRRegClass;
}
