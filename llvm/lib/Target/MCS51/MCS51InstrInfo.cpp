#include "MCS51InstrInfo.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Casting.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "MCS51GenInstrInfo.inc"

using namespace llvm;

MCS51InstrInfo::MCS51InstrInfo(const MCS51Subtarget &STI)
    : MCS51GenInstrInfo(STI, RI, MCS51::ADJCALLSTACKDOWN,
                      MCS51::ADJCALLSTACKUP, ~0u, MCS51::RET), RI() {}

void MCS51InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 const DebugLoc &DL, Register DestReg,
                                 Register SrcReg, bool KillSrc, bool,
                                 bool) const {
  if (DestReg == SrcReg)
    return;
  unsigned Opcode;
  auto IsPair = [](Register Reg) {
    return Reg == MCS51::R2R3 || Reg == MCS51::R4R5 || Reg == MCS51::R6R7;
  };
  auto Lo = [&](Register Pair) {
    return Pair == MCS51::R2R3 ? MCS51::R2
                               : Pair == MCS51::R4R5 ? MCS51::R4 : MCS51::R6;
  };
  auto Hi = [&](Register Pair) {
    return Pair == MCS51::R2R3 ? MCS51::R3
                               : Pair == MCS51::R4R5 ? MCS51::R5 : MCS51::R7;
  };
  // Byte copies to and from one half of DPTR.
  auto IsHalf = [](Register Reg) {
    return Reg == MCS51::DPL || Reg == MCS51::DPH;
  };
  // Imaginary registers are bytes of direct RAM, so they move with ordinary
  // direct-addressed instructions.
  auto IsImag8 = [](Register Reg) {
    return MCS51::MCS51Imag8RegClass.contains(Reg);
  };
  auto IsImagPair = [](Register Reg) {
    return MCS51::MCS51GPR16RegClass.contains(Reg);
  };
  auto ImagLo = [&](Register Pair) {
    return RI.getSubReg(Pair, MCS51::sub_lo);
  };
  auto ImagHi = [&](Register Pair) {
    return RI.getSubReg(Pair, MCS51::sub_hi);
  };
  auto MoveImagByte = [&](Register Dst, Register Src, bool Kill) {
    if (IsImag8(Dst) && IsImag8(Src))
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_IM), Dst)
          .addReg(Src, getKillRegState(Kill));
    else if (IsImag8(Dst) && IsHalf(Src))
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_DIRECT), Dst)
          .addImm(Src == MCS51::DPL ? 0x82 : 0x83)
          .addReg(Src, RegState::Implicit);
    else if (IsImag8(Src) && IsHalf(Dst))
      BuildMI(MBB, MI, DL, get(MCS51::MOV_DIRECT_IM))
          .addImm(Dst == MCS51::DPL ? 0x82 : 0x83)
          .addReg(Src, getKillRegState(Kill))
          .addReg(Dst, RegState::ImplicitDefine);
    else if (IsImag8(Dst) && Src == MCS51::A)
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_A), Dst)
          .addReg(Src, getKillRegState(Kill));
    else if (IsImag8(Dst) && MCS51::MCS51GPR8RegClass.contains(Src))
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_RN), Dst)
          .addReg(Src, getKillRegState(Kill));
    else if (IsImag8(Src) && Dst == MCS51::A)
      BuildMI(MBB, MI, DL, get(MCS51::MOV_A_IM), Dst)
          .addReg(Src, getKillRegState(Kill));
    else if (IsImag8(Src) && MCS51::MCS51GPR8RegClass.contains(Dst))
      BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_IM), Dst)
          .addReg(Src, getKillRegState(Kill));
    else
      llvm_unreachable("unsupported imaginary byte copy");
  };
  if (IsImag8(DestReg) || IsImag8(SrcReg)) {
    MoveImagByte(DestReg, SrcReg, KillSrc);
    return;
  }
  if (IsImagPair(DestReg) && IsImagPair(SrcReg)) {
    MoveImagByte(ImagLo(DestReg), ImagLo(SrcReg), KillSrc);
    MoveImagByte(ImagHi(DestReg), ImagHi(SrcReg), KillSrc);
    return;
  }
  if (DestReg == MCS51::DPTR && IsImagPair(SrcReg)) {
    MoveImagByte(MCS51::DPL, ImagLo(SrcReg), KillSrc);
    MoveImagByte(MCS51::DPH, ImagHi(SrcReg), KillSrc);
    return;
  }
  if (IsImagPair(DestReg) && SrcReg == MCS51::DPTR) {
    MoveImagByte(ImagLo(DestReg), MCS51::DPL, false);
    MoveImagByte(ImagHi(DestReg), MCS51::DPH, false);
    return;
  }
  // A byte copied into a word register zero-extends; out of one it takes the
  // low byte.
  if (IsImagPair(DestReg) &&
      (SrcReg == MCS51::A || MCS51::MCS51GPR8RegClass.contains(SrcReg))) {
    MoveImagByte(ImagLo(DestReg), SrcReg, KillSrc);
    BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_IMM), ImagHi(DestReg)).addImm(0);
    return;
  }
  if (IsImagPair(SrcReg) && (DestReg == MCS51::A ||
                             MCS51::MCS51GPR8RegClass.contains(DestReg))) {
    MoveImagByte(DestReg, ImagLo(SrcReg), KillSrc);
    return;
  }
  if (IsHalf(SrcReg) && MCS51::MCS51GPR8RegClass.contains(DestReg)) {
    BuildMI(MBB, MI, DL,
            get(SrcReg == MCS51::DPL ? MCS51::MOV_RN_DPL : MCS51::MOV_RN_DPH),
            DestReg);
    return;
  }
  if (IsHalf(SrcReg) && DestReg == MCS51::A) {
    BuildMI(MBB, MI, DL,
            get(SrcReg == MCS51::DPL ? MCS51::MOV_A_DPL : MCS51::MOV_A_DPH),
            MCS51::A)
        .addReg(MCS51::DPTR);
    return;
  }
  if (IsHalf(DestReg) && MCS51::MCS51GPR8RegClass.contains(SrcReg)) {
    BuildMI(MBB, MI, DL,
            get(DestReg == MCS51::DPL ? MCS51::MOV_DPL_RN : MCS51::MOV_DPH_RN))
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (IsHalf(DestReg) && SrcReg == MCS51::A) {
    BuildMI(MBB, MI, DL,
            get(DestReg == MCS51::DPL ? MCS51::MOV_DPL_A : MCS51::MOV_DPH_A));
    return;
  }
  if (DestReg == MCS51::DPTR && IsPair(SrcReg)) {
    BuildMI(MBB, MI, DL, get(MCS51::MOV_DPL_RN))
        .addReg(Lo(SrcReg), getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_DPH_RN))
        .addReg(Hi(SrcReg), getKillRegState(KillSrc));
    return;
  }
  if (IsPair(DestReg) && SrcReg == MCS51::DPTR) {
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_DPL), Lo(DestReg));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_DPH), Hi(DestReg));
    return;
  }
  if (IsPair(DestReg) && IsPair(SrcReg)) {
    BuildMI(MBB, MI, DL, get(MCS51::MOV_A_RN))
        .addReg(Lo(SrcReg), getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_A), Lo(DestReg));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_A_RN))
        .addReg(Hi(SrcReg), getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_A), Hi(DestReg));
    return;
  }
  // A byte copy from a word register takes the low byte, as it does for DPTR.
  if (IsPair(SrcReg) && (DestReg == MCS51::A ||
                         MCS51::MCS51GPR8RegClass.contains(DestReg))) {
    BuildMI(MBB, MI, DL, get(MCS51::MOV_A_RN))
        .addReg(Lo(SrcReg), getKillRegState(KillSrc));
    if (DestReg != MCS51::A)
      BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_A), DestReg);
    return;
  }
  if (IsPair(DestReg) && (SrcReg == MCS51::A ||
                          MCS51::MCS51GPR8RegClass.contains(SrcReg))) {
    if (SrcReg != MCS51::A)
      BuildMI(MBB, MI, DL, get(MCS51::MOV_A_RN))
          .addReg(SrcReg, getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_A), Lo(DestReg));
    BuildMI(MBB, MI, DL, get(MCS51::CLR_A));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_A), Hi(DestReg));
    return;
  }
  if (DestReg == MCS51::A && MCS51::MCS51GPR8RegClass.contains(SrcReg)) {
    Opcode = MCS51::MOV_A_RN;
    BuildMI(MBB, MI, DL, get(Opcode)).addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (DestReg == MCS51::DPTR &&
      (SrcReg == MCS51::A || MCS51::MCS51GPR8RegClass.contains(SrcReg))) {
    if (SrcReg == MCS51::A)
      BuildMI(MBB, MI, DL, get(MCS51::MOV_DPL_A));
    else
      BuildMI(MBB, MI, DL, get(MCS51::MOV_DPL_RN))
          .addReg(SrcReg, getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(MCS51::MOV_DIRECT_IMM))
        .addImm(0x83)
        .addImm(0)
        .addReg(MCS51::DPTR, RegState::ImplicitDefine);
    return;
  }
  if (SrcReg == MCS51::A && MCS51::MCS51GPR8RegClass.contains(DestReg)) {
    Opcode = MCS51::MOV_RN_A;
    BuildMI(MBB, MI, DL, get(Opcode), DestReg);
    return;
  }
  if (SrcReg == MCS51::DPTR &&
      MCS51::MCS51GPR8RegClass.contains(DestReg)) {
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_DPL), DestReg);
    return;
  }
  if (MCS51::MCS51GPR8RegClass.contains(DestReg) &&
      MCS51::MCS51GPR8RegClass.contains(SrcReg)) {
    // Through A when A is dead (two one-byte instructions the peepholes can
    // chain); otherwise MOV Rn,direct reads the source by its address in
    // register bank 0 and leaves a live A alone.
    if (MBB.computeRegisterLiveness(&RI, MCS51::A, MI, 16) ==
        MachineBasicBlock::LQR_Dead) {
      BuildMI(MBB, MI, DL, get(MCS51::MOV_A_RN))
          .addReg(SrcReg, getKillRegState(KillSrc));
      BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_A), DestReg);
      return;
    }
    BuildMI(MBB, MI, DL, get(MCS51::MOV_RN_DIRECT), DestReg)
        .addImm(RI.getEncodingValue(SrcReg))
        .addReg(SrcReg, getKillRegState(KillSrc) | RegState::Implicit);
    return;
  }
  llvm_unreachable("unsupported MCS-51 physical register copy");
}

void MCS51InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC, Register,
    MachineInstr::MIFlag Flags) const {
  bool IsByte = RC == &MCS51::MCS51GPR8RegClass;
  bool IsImagByte = MCS51::MCS51Imag8RegClass.hasSubClassEq(RC);
  bool IsIndirectByte = RC == &MCS51::MCS51Indirect8RegClass;
  bool IsAccumulator = RC == &MCS51::MCS51ARegRegClass;
  // The allocator also asks for subclasses of the word class when a
  // subregister restricts the possible pairs.
  bool IsWord = MCS51::MCS51GPR16RegClass.hasSubClassEq(RC) ||
                MCS51::MCS51PTRRegClass.hasSubClassEq(RC);
  if (!IsByte && !IsImagByte && !IsIndirectByte && !IsAccumulator && !IsWord)
    report_fatal_error(Twine("unsupported MCS-51 spill register class ") +
                       RI.getRegClassName(RC));
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));
  unsigned Opcode = IsByte ? MCS51::SPILL_STORE8
                           : IsImagByte ? MCS51::SPILL_STORE_IM
                           : IsIndirectByte ? MCS51::SPILL_STORE_INDIRECT8
                           : IsAccumulator ? MCS51::SPILL_STORE_A8
                                            : MCS51::SPILL_STORE16;
  BuildMI(MBB, MI, DebugLoc(), get(Opcode))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addReg(SrcReg, getKillRegState(IsKill))
      .addMemOperand(MMO)
      .setMIFlags(Flags);
}

void MCS51InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register, unsigned SubReg,
    MachineInstr::MIFlag Flags) const {
  bool IsByte = RC == &MCS51::MCS51GPR8RegClass;
  bool IsImagByte = MCS51::MCS51Imag8RegClass.hasSubClassEq(RC);
  bool IsIndirectByte = RC == &MCS51::MCS51Indirect8RegClass;
  bool IsAccumulator = RC == &MCS51::MCS51ARegRegClass;
  // The allocator also asks for subclasses of the word class when a
  // subregister restricts the possible pairs.
  bool IsWord = MCS51::MCS51GPR16RegClass.hasSubClassEq(RC) ||
                MCS51::MCS51PTRRegClass.hasSubClassEq(RC);
  if ((!IsByte && !IsImagByte && !IsIndirectByte && !IsAccumulator &&
       !IsWord) ||
      SubReg)
    report_fatal_error(Twine("unsupported MCS-51 reload register class ") +
                       RI.getRegClassName(RC));
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));
  unsigned Opcode = IsByte ? MCS51::SPILL_LOAD8
                           : IsImagByte ? MCS51::SPILL_LOAD_IM
                           : IsIndirectByte ? MCS51::SPILL_LOAD_INDIRECT8
                           : IsAccumulator ? MCS51::SPILL_LOAD_A8
                                            : MCS51::SPILL_LOAD16;
  BuildMI(MBB, MI, DebugLoc(), get(Opcode), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(MMO)
      .setMIFlags(Flags);
}

// Stack arguments are pushed and released one byte at a time by explicit
// instructions, so the call frame pseudos move nothing themselves. Frame
// indices materialized between two argument pushes must see only the bytes
// pushed so far. Stack growth is upward: growth is a negative adjustment.
int MCS51InstrInfo::getSPAdjust(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case MCS51::ADJCALLSTACKDOWN:
  case MCS51::ADJCALLSTACKUP:
    return 0;
  case MCS51::PUSH_DIRECT:
  case MCS51::PUSH_IM:
  case MCS51::PUSH_PSW:
    return -1;
  case MCS51::POP_DIRECT:
  case MCS51::POP_IM:
  case MCS51::POP_PSW:
  case MCS51::POP_ARG_SP:
    return 1;
  case MCS51::INC_DIRECT:
  case MCS51::DEC_DIRECT:
    if (MI.getOperand(0).isImm() && MI.getOperand(0).getImm() == 0x81)
      return MI.getOpcode() == MCS51::INC_DIRECT ? -1 : 1;
    return 0;
  default:
    return 0;
  }
}

bool MCS51InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();
  if (Opcode == MCS51::LDI16) {
    MachineBasicBlock &MBB = *MI.getParent();
    const DebugLoc &DL = MI.getDebugLoc();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Src = MI.getOperand(1);
    if (Src.isGlobal() && !isa<Function>(Src.getGlobal()) &&
        Dst != MCS51::DPTR) {
      // The bytes of a variable's address are loaded directly into the pair.
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_SYMLO),
              RI.getSubReg(Dst, MCS51::sub_lo))
          .addGlobalAddress(Src.getGlobal(), Src.getOffset());
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_SYMHI),
              RI.getSubReg(Dst, MCS51::sub_hi))
          .addGlobalAddress(Src.getGlobal(), Src.getOffset());
      MI.eraseFromParent();
      return true;
    }
    if (!Src.isImm()) {
      // A symbol address needs the 16-bit relocation that only MOV DPTR has.
      MachineInstrBuilder Load =
          BuildMI(MBB, MI, DL, get(MCS51::MOV_DPTR_IMM), MCS51::DPTR);
      Load.add(Src);
      if (Dst != MCS51::DPTR)
        copyPhysReg(MBB, MI, DL, Dst, MCS51::DPTR, false, false, false);
      MI.eraseFromParent();
      return true;
    }
    int64_t Imm = Src.getImm() & 0xffff;
    if (Dst == MCS51::DPTR) {
      BuildMI(MBB, MI, DL, get(MCS51::MOV_DPTR_IMM), MCS51::DPTR).addImm(Imm);
    } else {
      Register Lo = RI.getSubReg(Dst, MCS51::sub_lo);
      Register Hi = RI.getSubReg(Dst, MCS51::sub_hi);
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_IMM), Lo).addImm(Imm & 0xff);
      BuildMI(MBB, MI, DL, get(MCS51::MOV_IM_IMM), Hi).addImm(Imm >> 8);
    }
    MI.eraseFromParent();
    return true;
  }
  if (Opcode == MCS51::RET_A) {
    MachineBasicBlock &MBB = *MI.getParent();
    MachineBasicBlock::iterator I = MI.getIterator();
    const DebugLoc &DL = MI.getDebugLoc();
    Register RetVal = MI.getOperand(0).getReg();
    if (RetVal != MCS51::A && I != MBB.begin()) {
      MachineBasicBlock::iterator Prev = std::prev(I);
      if (Prev->getOpcode() == MCS51::MOV_RN_A &&
          Prev->getOperand(0).getReg() == RetVal) {
        Prev->eraseFromParent();
        RetVal = MCS51::A;
      }
    }
    if (RetVal != MCS51::A)
      BuildMI(MBB, I, DL, get(MCS51::MOV_A_RN))
          .addReg(RetVal, getKillRegState(MI.getOperand(0).isKill()));
    BuildMI(MBB, I, DL, get(MCS51::RET_NOA))
        .addReg(MCS51::A, RegState::Implicit);
    MI.eraseFromParent();
    return true;
  }
  if (Opcode == MCS51::STOREI8) {
    MachineBasicBlock &MBB = *MI.getParent();
    MachineBasicBlock::iterator I = MI.getIterator();
    const DebugLoc &DL = MI.getDebugLoc();
    Register Addr = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    BuildMI(MBB, I, DL, get(MCS51::MOV_A_RN)).addReg(Src);
    BuildMI(MBB, I, DL, get(MCS51::MOV_IND_RI_A)).addReg(Addr);
    MI.eraseFromParent();
    return true;
  }
  if (Opcode != MCS51::LOADSTACKARG8)
    return false;

  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const DebugLoc &DL = MI.getDebugLoc();
  Register Dst = MI.getOperand(0).getReg();
  int64_t Offset = static_cast<int8_t>(MI.getOperand(1).getImm()) -
                   MFI.getStackSize();
  if (Offset < -128 || Offset > 127)
    report_fatal_error("MCS-51 stack arguments exceed 128-byte displacement");

  auto I = MI.getIterator();
  BuildMI(MBB, I, DL, get(MCS51::MOV_A_DIRECT), MCS51::A).addImm(0x81);
  BuildMI(MBB, I, DL, get(MCS51::ADD_A_IMM), MCS51::A).addImm(Offset);
  BuildMI(MBB, I, DL, get(MCS51::MOV_RN_A))
      .addReg(MCS51::R0, RegState::Define);
  BuildMI(MBB, I, DL, get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R0);

  BuildMI(MBB, I, DL, get(MCS51::MOV_RN_A), Dst);

  MI.eraseFromParent();
  return true;
}

namespace {
bool isMCS51CondBranch(unsigned Opcode) {
  switch (Opcode) {
  case MCS51::JZ:
  case MCS51::JNZ:
  case MCS51::JC:
  case MCS51::JNC:
  case MCS51::JB:
  case MCS51::JNB:
  case MCS51::JBC:
  case MCS51::DJNZ_RN:
  case MCS51::DJNZ_DIRECT:
  case MCS51::CJNE_A_IMM:
  case MCS51::CJNE_A_DIRECT:
  case MCS51::CJNE_IND_R0:
  case MCS51::CJNE_IND_R1:
  case MCS51::CJNE_RN:
    return true;
  default:
    return false;
  }
}

bool isMCS51UncondBranch(unsigned Opcode) {
  return Opcode == MCS51::LJMP || Opcode == MCS51::SJMP ||
         Opcode == MCS51::AJMP;
}
} // namespace

bool MCS51InstrInfo::analyzeBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *&TBB, MachineBasicBlock *&FBB,
    SmallVectorImpl<MachineOperand> &Cond, bool AllowModify) const {
  auto AppendCondition = [&](const MachineInstr &MI) {
    unsigned Opcode = MI.getOpcode();
    Cond.push_back(MachineOperand::CreateImm(Opcode));
    if (Opcode == MCS51::JZ || Opcode == MCS51::JNZ ||
        Opcode == MCS51::JC || Opcode == MCS51::JNC)
      return;
    for (const MachineOperand &MO : MI.operands())
      if (!MO.isMBB() && !(MO.isReg() && MO.isImplicit()))
        Cond.push_back(MO);
  };

  TBB = FBB = nullptr;
  Cond.clear();
  auto I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return false;

  unsigned Opcode = I->getOpcode();
  if (isMCS51UncondBranch(Opcode)) {
    TBB = I->getOperand(0).getMBB();
    if (I == MBB.begin())
      return false;
    auto Prev = std::prev(I);
    while (Prev != MBB.begin() && Prev->isDebugInstr())
      --Prev;
    if (isMCS51CondBranch(Prev->getOpcode())) {
      FBB = TBB;
      TBB = getBranchDestBlock(*Prev);
      if (!TBB)
        return true;
      AppendCondition(*Prev);
    }
    if (AllowModify && !Cond.empty() && MBB.isLayoutSuccessor(FBB)) {
      I->eraseFromParent();
      FBB = nullptr;
    }
    return false;
  }

  if (isMCS51CondBranch(Opcode)) {
    TBB = getBranchDestBlock(*I);
    if (!TBB)
      return true;
    AppendCondition(*I);
    return false;
  }

  return I->isBranch();
}

unsigned MCS51InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  unsigned Removed = 0;
  while (true) {
    auto I = MBB.getLastNonDebugInstr();
    if (I == MBB.end())
      break;
    unsigned Opcode = I->getOpcode();
    if (!isMCS51CondBranch(Opcode) && !isMCS51UncondBranch(Opcode))
      break;
    if (BytesRemoved)
      *BytesRemoved += get(Opcode).getSize();
    I->eraseFromParent();
    ++Removed;
  }
  return Removed;
}

unsigned MCS51InstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL,
    int *BytesAdded) const {
  assert(TBB && "insertBranch requires a target");
  if (BytesAdded)
    *BytesAdded = 0;
  unsigned Count = 0;
  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(MCS51::SJMP)).addMBB(TBB);
    Count = 1;
    if (BytesAdded)
      *BytesAdded = 2;
  } else {
    assert(!Cond.empty() && Cond[0].isImm() &&
           "unsupported MCS-51 branch condition");
    unsigned Opcode = Cond[0].getImm();
    assert(isMCS51CondBranch(Opcode) && "invalid MCS-51 branch condition");
    if (Cond.size() == 1) {
      BuildMI(&MBB, DL, get(Opcode)).addMBB(TBB);
    } else {
      MachineInstrBuilder Branch = BuildMI(&MBB, DL, get(Opcode));
      for (unsigned I = 1; I != Cond.size(); ++I)
        Branch.add(Cond[I]);
      Branch.addMBB(TBB);
    }
    Count = 1;
    if (BytesAdded)
      *BytesAdded = get(Opcode).getSize();
    if (FBB) {
      BuildMI(&MBB, DL, get(MCS51::SJMP)).addMBB(FBB);
      ++Count;
      if (BytesAdded)
        *BytesAdded += 2;
    }
  }
  return Count;
}

bool MCS51InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  if (Cond.size() != 1 || !Cond[0].isImm())
    return true;
  switch (Cond[0].getImm()) {
  case MCS51::JZ: Cond[0].setImm(MCS51::JNZ); break;
  case MCS51::JNZ: Cond[0].setImm(MCS51::JZ); break;
  case MCS51::JC: Cond[0].setImm(MCS51::JNC); break;
  case MCS51::JNC: Cond[0].setImm(MCS51::JC); break;
  default:
    return true;
  }
  return false;
}

unsigned MCS51InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isInlineAsm()) {
    const MachineFunction *MF = MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              MF->getTarget().getMCAsmInfo());
  }
  return get(MI.getOpcode()).getSize();
}

MachineBasicBlock *
MCS51InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  for (const MachineOperand &MO : MI.operands())
    if (MO.isMBB())
      return MO.getMBB();
  return nullptr;
}

bool MCS51InstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                           int64_t BrOffset) const {
  if (isMCS51CondBranch(BranchOpc) || BranchOpc == MCS51::SJMP)
    // BranchRelaxation measures from the instruction start, while the 8051
    // displacement is relative to the byte after the branch instruction.
    return isInt<8>(BrOffset - get(BranchOpc).getSize());
  if (BranchOpc == MCS51::LJMP || BranchOpc == MCS51::AJMP)
    return true;
  llvm_unreachable("unexpected MCS-51 branch opcode");
}

void MCS51InstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                         MachineBasicBlock &NewDestBB,
                                         MachineBasicBlock &,
                                         const DebugLoc &DL, int64_t,
                                         RegScavenger *) const {
  BuildMI(&MBB, DL, get(MCS51::LJMP)).addMBB(&NewDestBB);
}
