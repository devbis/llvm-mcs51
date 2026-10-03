#include "MCS51FrameLowering.h"
#include "MCS51InstrInfo.h"
#include "MCS51RegisterInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include <algorithm>

using namespace llvm;

MCS51FrameLowering::MCS51FrameLowering()
    : TargetFrameLowering(StackGrowsUp, Align(1), 1) {}

bool MCS51FrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *,
    std::vector<CalleeSavedInfo> &) const {
  // Frame objects are plain bytes on the 8-bit stack. Source alignments of 2
  // for 16-bit objects would only add padding to the 255-byte stack window.
  MachineFrameInfo &MFI = MF.getFrameInfo();
  for (int I = 0, E = MFI.getObjectIndexEnd(); I < E; ++I)
    if (!MFI.isDeadObjectIndex(I))
      MFI.setObjectAlignment(I, Align(1));
  // Callee-saved R registers are pushed directly; they do not need frame slots.
  return true;
}

bool MCS51FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *) const {
  const TargetInstrInfo &TII = *MBB.getParent()->getSubtarget().getInstrInfo();
  for (const CalleeSavedInfo &CS : CSI) {
    Register Reg = CS.getReg();
    unsigned Direct = Reg.id() - MCS51::R0;
    MBB.addLiveIn(Reg);
    BuildMI(MBB, MI, DebugLoc(), TII.get(MCS51::PUSH_DIRECT))
        .addImm(Direct)
        .addReg(Reg, RegState::Implicit)
        .addReg(MCS51::SP, RegState::ImplicitDefine)
        .addReg(MCS51::SP, RegState::Implicit)
        .setMIFlag(MachineInstr::FrameSetup);
  }
  return true;
}

bool MCS51FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *) const {
  const TargetInstrInfo &TII = *MBB.getParent()->getSubtarget().getInstrInfo();
  for (CalleeSavedInfo &CS : llvm::reverse(CSI)) {
    Register Reg = CS.getReg();
    unsigned Direct = Reg.id() - MCS51::R0;
    BuildMI(MBB, MI, DebugLoc(), TII.get(MCS51::POP_DIRECT))
        .addImm(Direct)
        .addReg(Reg, RegState::ImplicitDefine)
        .addReg(MCS51::SP, RegState::ImplicitDefine)
        .addReg(MCS51::SP, RegState::Implicit)
        .setMIFlag(MachineInstr::FrameDestroy);
  }
  return true;
}

static void emitStackAdjustment(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const TargetInstrInfo &TII, uint64_t Amount,
                                bool Deallocate, bool PreserveA = false) {
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
  if (Deallocate && PreserveA) {
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

static SmallVector<unsigned, 13>
getInterruptSaveAddresses(const MachineFunction &MF, bool Reverse) {
  // An interrupt may arrive with arbitrary values in the register bank, so
  // save every architectural register that the handler actually clobbers.
  // Calls carry their complete clobber list and therefore remain conservative.
  bool SavePSW = false, SaveA = false, SaveB = false;
  bool SaveDPL = false, SaveDPH = false;
  bool SaveR[8] = {};
  auto Mark = [&](MCRegister Reg) {
    switch (Reg) {
    case MCS51::PSW: SavePSW = true; break;
    case MCS51::C: SavePSW = true; break;
    case MCS51::A: SaveA = true; break;
    case MCS51::B: SaveB = true; break;
    case MCS51::DPTR: SaveDPL = SaveDPH = true; break;
    case MCS51::DPL: SaveDPL = true; break;
    case MCS51::DPH: SaveDPH = true; break;
    case MCS51::R0: SaveR[0] = true; break;
    case MCS51::R1: SaveR[1] = true; break;
    case MCS51::R2: SaveR[2] = true; break;
    case MCS51::R3: SaveR[3] = true; break;
    case MCS51::R4: SaveR[4] = true; break;
    case MCS51::R5: SaveR[5] = true; break;
    case MCS51::R6: SaveR[6] = true; break;
    case MCS51::R7: SaveR[7] = true; break;
    case MCS51::R2R3: SaveR[2] = SaveR[3] = true; break;
    case MCS51::R4R5: SaveR[4] = SaveR[5] = true; break;
    case MCS51::R6R7: SaveR[6] = SaveR[7] = true; break;
    default: break;
    }
  };
  auto MarkDirectWrite = [&](unsigned Address) {
    switch (Address) {
    case 0xd0:
      // A direct PSW write can also change the active register bank.
      SavePSW = true;
      for (bool &Save : SaveR)
        Save = true;
      break;
    case 0xe0: Mark(MCS51::A); break;
    case 0xf0: Mark(MCS51::B); break;
    case 0x82: Mark(MCS51::DPL); break;
    case 0x83: Mark(MCS51::DPH); break;
    default:
      if (Address < 8)
        SaveR[Address] = true;
      break;
    }
  };
  for (const MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : MBB) {
      for (const MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
          Mark(MO.getReg());
      if (MI.getFlag(MachineInstr::FrameSetup) ||
          MI.getFlag(MachineInstr::FrameDestroy) || MI.getNumOperands() == 0 ||
          !MI.getOperand(0).isImm())
        continue;
      switch (MI.getOpcode()) {
      case MCS51::MOV_DIRECT_A:
      case MCS51::ANL_DIRECT_A:
      case MCS51::ORL_DIRECT_A:
      case MCS51::XRL_DIRECT_A:
      case MCS51::MOV_DIRECT_IMM:
      case MCS51::ANL_DIRECT_IMM:
      case MCS51::ORL_DIRECT_IMM:
      case MCS51::XRL_DIRECT_IMM:
      case MCS51::MOV_DIRECT_DIRECT:
      case MCS51::MOV_DIRECT_RN:
      case MCS51::INC_DIRECT:
      case MCS51::DEC_DIRECT:
      case MCS51::POP_DIRECT:
        MarkDirectWrite(MI.getOperand(0).getImm());
        break;
      default:
        break;
      }
    }

  SmallVector<unsigned, 13> Addresses;
  if (SavePSW) Addresses.push_back(0xd0);
  if (SaveA) Addresses.push_back(0xe0);
  if (SaveB) Addresses.push_back(0xf0);
  if (SaveDPL) Addresses.push_back(0x82);
  if (SaveDPH) Addresses.push_back(0x83);
  for (unsigned I = 0; I != 8; ++I)
    if (SaveR[I]) Addresses.push_back(I);
  if (Reverse)
    std::reverse(Addresses.begin(), Addresses.end());
  return Addresses;
}

void MCS51FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  if (StackSize + MF.getFrameInfo().getCalleeSavedInfo().size() > 255)
    report_fatal_error(Twine("MCS-51 stack frame exceeds 255-byte stack address "
                             "space in '") +
                       MF.getName() + "': " + Twine(StackSize) +
                       " bytes of locals and " +
                       Twine(MF.getFrameInfo().getCalleeSavedInfo().size()) +
                       " saved registers");
  bool IsInterrupt = MF.getFunction().hasFnAttribute("interrupt");
  MachineBasicBlock::iterator I = MBB.begin();
  while (I != MBB.end() &&
         (I->isDebugInstr() || I->getFlag(MachineInstr::FrameSetup)))
    ++I;
  if (IsInterrupt) {
    // The 8051 hardware already saved the return PC. Preserve the subset of
    // registers the allocated handler can change.
    for (unsigned Address : getInterruptSaveAddresses(MF, false))
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
  if (I != MBB.end() && I->getOpcode() == MCS51::RET_A &&
      I->getNumOperands() && I->getOperand(0).isReg()) {
    Register RetReg = I->getOperand(0).getReg();
    bool IsSavedReturnReg = RetReg == MCS51::R2 || RetReg == MCS51::R3 ||
                            RetReg == MCS51::R4 || RetReg == MCS51::R5 ||
                            RetReg == MCS51::R6 || RetReg == MCS51::R7;
    if (IsSavedReturnReg) {
      MachineBasicBlock::iterator Restore = I;
      for (auto It = MBB.begin(); It != I; ++It)
        if (It->getFlag(MachineInstr::FrameDestroy)) {
          Restore = It;
          break;
        }
      BuildMI(MBB, Restore, I->getDebugLoc(), TII.get(MCS51::MOV_A_RN))
          .addReg(RetReg);
      BuildMI(MBB, Restore, I->getDebugLoc(), TII.get(MCS51::MOV_RN_A),
              MCS51::R0);
      I->getOperand(0).setReg(MCS51::R0);
    }
  }
  bool PreserveA = I != MBB.end() && I->getOpcode() == MCS51::RET_A &&
                   I->getNumOperands() && I->getOperand(0).isReg() &&
                   I->getOperand(0).getReg() == MCS51::A;
  // The freestanding entry point returns to startup's halt loop. No caller
  // resumes with its stack, so releasing main's final frame is unnecessary.
  if (MF.getFunction().getName() != "main" ||
      !MF.getTarget().getTargetCPU().equals_insensitive("cc2530"))
    emitStackAdjustment(MBB, I, TII, StackSize, /*Deallocate=*/true,
                        PreserveA);
  if (MF.getFunction().hasFnAttribute("interrupt")) {
    for (unsigned Address : getInterruptSaveAddresses(MF, true))
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
