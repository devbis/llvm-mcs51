#include "MCS51InstrInfo.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "MCS51GenInstrInfo.inc"

using namespace llvm;

MCS51InstrInfo::MCS51InstrInfo(const MCS51Subtarget &STI)
    : MCS51GenInstrInfo(STI, RI, ~0u, ~0u, ~0u, MCS51::RET), RI() {}

void MCS51InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 const DebugLoc &DL, Register DestReg,
                                 Register SrcReg, bool KillSrc, bool,
                                 bool) const {
  if (DestReg == SrcReg)
    return;
  unsigned Opcode;
  if (DestReg == MCS51::A && MCS51::MCS51GPR8RegClass.contains(SrcReg)) {
    Opcode = MCS51::MOV_A_RN;
    BuildMI(MBB, MI, DL, get(Opcode)).addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (SrcReg == MCS51::A && MCS51::MCS51GPR8RegClass.contains(DestReg)) {
    Opcode = MCS51::MOV_RN_A;
    BuildMI(MBB, MI, DL, get(Opcode), DestReg);
    return;
  }
  if (MCS51::MCS51GPR8RegClass.contains(DestReg) &&
      MCS51::MCS51GPR8RegClass.contains(SrcReg)) {
    Opcode = MCS51::MOV_RN_RM;
    BuildMI(MBB, MI, DL, get(Opcode), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  llvm_unreachable("unsupported MCS-51 physical register copy");
}
