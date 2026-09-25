#include "MCS51InstrInfo.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

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

void MCS51InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC, Register,
    MachineInstr::MIFlag Flags) const {
  bool IsByte = RC == &MCS51::MCS51GPR8RegClass;
  bool IsWord = RC == &MCS51::MCS51PTRRegClass;
  if (!IsByte && !IsWord)
    llvm_unreachable("unsupported MCS-51 spill register class");
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));
  BuildMI(MBB, MI, DebugLoc(), get(IsByte ? MCS51::SPILL_STORE8
                                          : MCS51::SPILL_STORE16))
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
  bool IsWord = RC == &MCS51::MCS51PTRRegClass;
  if ((!IsByte && !IsWord) || SubReg)
    llvm_unreachable("unsupported MCS-51 reload register class");
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));
  BuildMI(MBB, MI, DebugLoc(), get(IsByte ? MCS51::SPILL_LOAD8
                                          : MCS51::SPILL_LOAD16), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(MMO)
      .setMIFlags(Flags);
}

bool MCS51InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();
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
  return Opcode == MCS51::JZ || Opcode == MCS51::JNZ ||
         Opcode == MCS51::JC || Opcode == MCS51::JNC;
}

bool isMCS51UncondBranch(unsigned Opcode) {
  return Opcode == MCS51::LJMP || Opcode == MCS51::SJMP;
}
} // namespace

bool MCS51InstrInfo::analyzeBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *&TBB, MachineBasicBlock *&FBB,
    SmallVectorImpl<MachineOperand> &Cond, bool AllowModify) const {
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
      TBB = Prev->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(Prev->getOpcode()));
    }
    if (AllowModify && !Cond.empty() && MBB.isLayoutSuccessor(FBB)) {
      I->eraseFromParent();
      FBB = nullptr;
    }
    return false;
  }

  if (isMCS51CondBranch(Opcode)) {
    TBB = I->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(Opcode));
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
    BuildMI(&MBB, DL, get(MCS51::LJMP)).addMBB(TBB);
    Count = 1;
    if (BytesAdded)
      *BytesAdded = 3;
  } else {
    assert(Cond.size() == 1 && Cond[0].isImm() &&
           "unsupported MCS-51 branch condition");
    unsigned Opcode = Cond[0].getImm();
    assert(isMCS51CondBranch(Opcode) && "invalid MCS-51 branch condition");
    BuildMI(&MBB, DL, get(Opcode)).addMBB(TBB);
    Count = 1;
    if (BytesAdded)
      *BytesAdded = get(Opcode).getSize();
    if (FBB) {
      BuildMI(&MBB, DL, get(MCS51::LJMP)).addMBB(FBB);
      ++Count;
      if (BytesAdded)
        *BytesAdded += 3;
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
