#include "MCS51RegisterInfo.h"
#include "MCS51.h"
#include "MCS51InstrInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "MCS51GenRegisterInfo.inc"

MCS51RegisterInfo::MCS51RegisterInfo() : MCS51GenRegisterInfo(MCS51::PC) {}

const MCPhysReg *
MCS51RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  // The freestanding C entry point is entered by startup code and never
  // returns to a C caller. It does not need to preserve the normal callee-save
  // register bank around its body.
  static const MCPhysReg MainSaveList[] = {0};
  static const MCPhysReg I32ReturnSaveList[] = {MCS51::R2, MCS51::R3, 0};
  static const MCPhysReg I64ReturnSaveList[] = {0};
  if (MF && MF->getFunction().getName() == "main" &&
      MF->getTarget().getTargetCPU().equals_insensitive("cc2530"))
    return MainSaveList;
  if (MF) {
    const Type *RetTy = MF->getFunction().getReturnType();
    if (RetTy->isIntegerTy(64))
      return I64ReturnSaveList;
    if (RetTy->isIntegerTy(32) || RetTy->isFloatTy())
      return I32ReturnSaveList;
  }
  return CSR_MCS51_SaveList;
}

const uint32_t *MCS51RegisterInfo::getCallPreservedMask(
    const MachineFunction &, CallingConv::ID) const {
  return CSR_MCS51_RegMask;
}

BitVector MCS51RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
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
  bool UsesDirectWordTransfer = false;
  if (Offset < -256 || Offset > 255)
    report_fatal_error("MCS-51 stack frame exceeds 256-byte displacement");

  auto I = MI->getIterator();
  auto EmitAddressAtOffset = [&](int64_t AddressOffset) {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A).addImm(0x81);
    BuildMI(MBB, I, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A)
        .addImm(static_cast<uint8_t>(AddressOffset));
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(MCS51::R1, RegState::Define);
  };
  auto EmitAddress = [&]() {
    if (Offset < -2 || Offset > 2) {
      EmitAddressAtOffset(Offset);
      return;
    }
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_DIRECT), MCS51::R1)
        .addImm(0x81);
    for (int64_t N = Offset; N > 0; --N)
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
    for (int64_t N = Offset; N < 0; ++N)
      BuildMI(MBB, I, DL, TII.get(MCS51::DEC_RN), MCS51::R1)
          .addReg(MCS51::R1);
  };
  auto OffsetAfterPush = [&](int64_t Count) {
    return static_cast<uint8_t>(Offset - Count);
  };

  if (MI->getOpcode() == MCS51::FRAMEADDR8) {
    EmitAddressAtOffset(Offset);
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), MI->getOperand(0).getReg());
    MI->eraseFromParent();
    return true;
  }

  if (MI->getOpcode() == MCS51::FRAMEADDR8_INDEX) {
    Register Index = MI->getOperand(FIOperandNum + 2).getReg();
    Register Dst = MI->getOperand(0).getReg();
    EmitAddressAtOffset(Offset);
    BuildMI(MBB, I, DL, TII.get(MCS51::ADD_A_RN)).addReg(Index);
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), Dst);
    MI->eraseFromParent();
    return true;
  }

  if (MI->getOpcode() == MCS51::FRAMEADDR_R1) {
    EmitAddressAtOffset(Offset);
    MI->eraseFromParent();
    return true;
  }

  if (MI->getOpcode() == MCS51::LOAD_FRAME8_INDEX ||
      MI->getOpcode() == MCS51::STORE_FRAME8_INDEX) {
    Register Src = MI->getOpcode() == MCS51::STORE_FRAME8_INDEX
                       ? MI->getOperand(FIOperandNum + 3).getReg()
                       : Register();
    EmitAddress();
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A).addImm(0x82);
    BuildMI(MBB, I, DL, TII.get(MCS51::ADD_A_RN)).addReg(MCS51::R1);
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(MCS51::R1, RegState::Define);
    if (MI->getOpcode() == MCS51::LOAD_FRAME8_INDEX) {
      Register Dst = MI->getOperand(0).getReg();
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), Dst);
    } else {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    }
    MI->eraseFromParent();
    return true;
  }

  if (MI->getOpcode() == MCS51::SPILL_LOAD8 ||
      MI->getOpcode() == MCS51::SPILL_LOAD_INDIRECT8 ||
      MI->getOpcode() == MCS51::LOAD_FRAME8 ||
      MI->getOpcode() == MCS51::SPILL_LOAD16 ||
      MI->getOpcode() == MCS51::SPILL_LOAD_A8 ||
      MI->getOpcode() == MCS51::LOAD_FRAME16) {
    Register Dst = MI->getOperand(0).getReg();
    if (MI->getOpcode() == MCS51::SPILL_LOAD_A8) {
      BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_PSW));
      EmitAddressAtOffset(OffsetAfterPush(1));
      BuildMI(MBB, I, DL, TII.get(MCS51::POP_PSW));
    } else {
      EmitAddress();
    }
    if (MI->getOpcode() == MCS51::SPILL_LOAD16) {
      UsesDirectWordTransfer = true;
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_R1_IND))
          .addImm(0x82)
          .addReg(MCS51::R1, RegState::Implicit);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_R1_IND))
          .addImm(0x83)
          .addReg(MCS51::R1, RegState::Implicit);
    } else {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R1);
    }
    if (MI->getOpcode() == MCS51::SPILL_LOAD_A8) {
      // The load itself leaves the byte in the accumulator register class.
    } else if (MI->getOpcode() == MCS51::SPILL_LOAD8 ||
        MI->getOpcode() == MCS51::SPILL_LOAD_INDIRECT8 ||
        MI->getOpcode() == MCS51::LOAD_FRAME8) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), Dst);
    } else if (MI->getOpcode() != MCS51::SPILL_LOAD16) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    }
  } else if (MI->getOpcode() == MCS51::SPILL_STORE8 ||
             MI->getOpcode() == MCS51::SPILL_STORE_INDIRECT8 ||
             MI->getOpcode() == MCS51::STORE_FRAME8 ||
             MI->getOpcode() == MCS51::SPILL_STORE16 ||
             MI->getOpcode() == MCS51::SPILL_STORE_A8 ||
             MI->getOpcode() == MCS51::STORE_FRAME16) {
    Register Src = MI->getOperand(FIOperandNum + 2).getReg();
    if (MI->getOpcode() == MCS51::SPILL_STORE16) {
      UsesDirectWordTransfer = true;
      EmitAddress();
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_R1_IND_DIRECT))
          .addImm(0x82)
          .addReg(MCS51::R1, RegState::Implicit)
          .addReg(MCS51::DPTR, RegState::Implicit);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_R1_IND_DIRECT))
          .addImm(0x83)
          .addReg(MCS51::R1, RegState::Implicit)
          .addReg(MCS51::DPTR, RegState::Implicit);
    } else if (MI->getOpcode() == MCS51::SPILL_STORE_A8) {
      if (Offset >= -2 && Offset <= 2) {
        EmitAddress();
      } else {
        // Save A and PSW while forming the address so the spill preserves
        // both the accumulator value and live condition flags.
        BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_DIRECT)).addImm(0xE0);
        BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_PSW));
        EmitAddressAtOffset(OffsetAfterPush(2));
        BuildMI(MBB, I, DL, TII.get(MCS51::POP_PSW));
        BuildMI(MBB, I, DL, TII.get(MCS51::POP_DIRECT)).addImm(0xE0);
      }
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    } else if (MI->getOpcode() == MCS51::SPILL_STORE8 ||
        MI->getOpcode() == MCS51::SPILL_STORE_INDIRECT8 ||
        MI->getOpcode() == MCS51::STORE_FRAME8) {
      EmitAddress();
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    } else {
      EmitAddress();
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x82);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x83);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    }
  } else {
    llvm_unreachable("unexpected MCS-51 frame-index instruction");
  }
  // Frame-index expansion inserts scratch uses after register allocation. The
  // kill flags computed for the spill pseudo no longer describe those uses.
  if (UsesDirectWordTransfer) {
    MF.getRegInfo().clearKillFlags(MCS51::A);
    MF.getRegInfo().clearKillFlags(MCS51::C);
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
