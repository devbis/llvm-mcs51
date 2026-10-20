#include "MCS51RegisterInfo.h"
#include "MCS51.h"
#include "MCS51InstrInfo.h"
#include "MCS51MachineFunctionInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "MCS51GenRegisterInfo.inc"

MCS51RegisterInfo::MCS51RegisterInfo() : MCS51GenRegisterInfo(MCS51::PC) {}

// Experimental: narrow the callee-saved set. This changes the C ABI, so every
// object that calls or is called by another (including the runtime library)
// must be built with the same value.
namespace {
enum class CSRSet { Default, R, None };
}
static cl::opt<CSRSet> CSRMode(
    "mcs51-csr", cl::Hidden, cl::init(CSRSet::Default),
    cl::desc("MCS51 callee-saved register set"),
    cl::values(clEnumValN(CSRSet::Default, "default", "R2-R7 and IM8-IM23"),
               clEnumValN(CSRSet::R, "r", "R2-R7"),
               clEnumValN(CSRSet::None, "none", "no callee-saved registers")));

bool llvm::mcs51UseCallRegMask() { return CSRMode != CSRSet::Default; }

// Experimental: pass further words in IP4-IP7 and further bytes in IM16-IM23
// instead of on the hardware stack. Like -mcs51-csr it changes the C ABI, and
// it requires those registers to be caller-saved.
static cl::opt<bool> WideArgs(
    "mcs51-wide-args", cl::Hidden, cl::init(false),
    cl::desc("pass arguments 5-8 (words) and 5-12 (bytes) in caller-saved "
             "imaginary registers; requires -mcs51-csr=r or none"));

bool llvm::mcs51WideArgs() {
  if (WideArgs && CSRMode == CSRSet::Default)
    report_fatal_error("-mcs51-wide-args requires -mcs51-csr=r or none: "
                       "IM8-IM23 are callee-saved by default");
  return WideArgs;
}

const MCPhysReg *
MCS51RegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  if (CSRMode == CSRSet::R)
    return CSR_MCS51_R_SaveList;
  if (CSRMode == CSRSet::None)
    return CSR_MCS51_None_SaveList;
  return CSR_MCS51_SaveList;
}

const uint32_t *MCS51RegisterInfo::getCallPreservedMask(
    const MachineFunction &, CallingConv::ID) const {
  if (CSRMode == CSRSet::R)
    return CSR_MCS51_R_RegMask;
  if (CSRMode == CSRSet::None)
    return CSR_MCS51_None_RegMask;
  return CSR_MCS51_RegMask;
}

BitVector MCS51RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(MCS51::PC);
  Reserved.set(MCS51::SP);
  Reserved.set(MCS51::PSW);
  // R1 is reserved as the indirect pointer for stack frame spill accesses.
  Reserved.set(MCS51::R1);
  // The halves of DPTR are byte subregisters for copies only.
  Reserved.set(MCS51::DPL);
  Reserved.set(MCS51::DPH);
  // DPTR is only ever used inside one expansion; word values live in the
  // imaginary pairs.
  Reserved.set(MCS51::DPTR);
  return Reserved;
}

bool MCS51RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *) const {
  auto PairLo = [&](Register Pair) { return getSubReg(Pair, MCS51::sub_lo); };
  auto PairHi = [&](Register Pair) { return getSubReg(Pair, MCS51::sub_hi); };

  MachineBasicBlock &MBB = *MI->getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const DebugLoc &DL = MI->getDebugLoc();
  int FI = MI->getOperand(FIOperandNum).getIndex();
  if (eliminateStaticFrameIndex(MI, FIOperandNum))
    return true;
  Register FrameReg;
  int64_t Offset = MF.getSubtarget().getFrameLowering()
                       ->getFrameIndexReference(MF, FI, FrameReg)
                       .getFixed();
  Offset += MI->getOperand(FIOperandNum + 1).getImm();
  // Stack arguments pushed for a call in progress move SP up, which puts the
  // frame further below it. PEI counts those pushes while it sees the call
  // sequence setup pseudo in the same block. If a CFG split put this block
  // inside the sequence, PEI starts from the full logical call frame size,
  // even though only some argument bytes may have been pushed on entry. Use
  // the exact per-block state recorded by MCS51CallFramePropagation and walk
  // this block's remaining call-sequence instructions to recover the actual
  // SP adjustment at this frame index.
  int64_t FrameSPAdj = SPAdj;
  const auto *CallState =
      MF.getInfo<MCS51MachineFunctionInfo>()->getCallFrameStateAtEntry(&MBB);
  if (MBB.getCallFrameSize() != 0 && (!CallState || !CallState->Active))
    report_fatal_error("missing MCS-51 call frame state for basic block");
  if (CallState && CallState->Active) {
    FrameSPAdj = CallState->SPAdj;
    bool InCallSequence = true;
    for (auto It = MBB.begin(); It != MI; ++It) {
      if (It->getOpcode() == MCS51::ADJCALLSTACKDOWN) {
        InCallSequence = true;
        FrameSPAdj = 0;
      } else if (It->getOpcode() == MCS51::ADJCALLSTACKUP) {
        InCallSequence = false;
        FrameSPAdj = 0;
      } else if (InCallSequence) {
        FrameSPAdj += TII.getSPAdjust(*It);
      }
    }
  }
  Offset += FrameSPAdj;
  bool UsesDirectWordTransfer = false;
  if (Offset < -256 || Offset > 255)
    report_fatal_error("MCS-51 stack frame exceeds 256-byte displacement");

  auto I = MI->getIterator();
  // A spilled pair may have only one defined half: the allocator spills the
  // whole pair when one of its lanes is live. Storing the other half is then
  // a use of an undefined register, which is flagged as such.
  auto ByteUndef = [&](Register Byte) {
    for (auto It = I; It != MBB.begin();) {
      --It;
      if (It->isDebugInstr())
        continue;
      if (It->modifiesRegister(Byte, this))
        return RegState{};
      if (It->killsRegister(Byte, this))
        return RegState::Undef;
    }
    for (const MachineBasicBlock::RegisterMaskPair &LI : MBB.liveins())
      if (regsOverlap(LI.PhysReg, Byte))
        return RegState{};
    return RegState::Undef;
  };
  auto EmitAddressAtOffset = [&](int64_t AddressOffset) {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A).addImm(0x81);
    BuildMI(MBB, I, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A)
        .addImm(static_cast<uint8_t>(AddressOffset));
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(MCS51::R1, RegState::Define);
  };
  auto EmitDirectStackAddress = [&](int64_t AddressOffset) {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_DIRECT), MCS51::R1)
        .addImm(0x81);
    for (int64_t N = AddressOffset; N > 0; --N)
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
    for (int64_t N = AddressOffset; N < 0; ++N)
      BuildMI(MBB, I, DL, TII.get(MCS51::DEC_RN), MCS51::R1)
          .addReg(MCS51::R1);
  };
  auto EmitAddress = [&]() {
    if (Offset < -2 || Offset > 2) {
      EmitAddressAtOffset(Offset);
      return;
    }
    EmitDirectStackAddress(Offset);
  };
  auto OffsetAfterPush = [&](int64_t Count) {
    return static_cast<uint8_t>(Offset - Count);
  };
  // Spill code inserted by the allocator can land between a write of A or C
  // and its reader, so it must leave both alone.
  auto IsLiveAfter = [&](MCRegister Reg) {
    for (auto It = std::next(MI); It != MBB.end(); ++It) {
      if (It->isDebugInstr())
        continue;
      if (It->readsRegister(Reg, this))
        return true;
      if (It->modifiesRegister(Reg, this))
        return false;
    }
    for (const MachineBasicBlock *Succ : MBB.successors())
      if (Succ->isLiveIn(Reg))
        return true;
    return false;
  };
  auto EmitSpillAddress = [&]() {
    if (Offset >= -2 && Offset <= 2) {
      EmitDirectStackAddress(Offset);
      return;
    }
    bool PreserveA = IsLiveAfter(MCS51::A);
    bool PreserveC = IsLiveAfter(MCS51::C);
    if (!PreserveA && !PreserveC) {
      EmitAddressAtOffset(Offset);
      return;
    }
    if (Offset >= -8 && Offset <= 8) {
      EmitDirectStackAddress(Offset);
      return;
    }
    unsigned Pushed = 0;
    if (PreserveA) {
      BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_DIRECT))
          .addImm(0xE0)
          .addReg(MCS51::A, RegState::Implicit);
      ++Pushed;
    }
    if (PreserveC) {
      BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_PSW))
          .addReg(MCS51::C, RegState::Implicit);
      ++Pushed;
    }
    EmitAddressAtOffset(OffsetAfterPush(Pushed));
    if (PreserveC)
      BuildMI(MBB, I, DL, TII.get(MCS51::POP_PSW))
          .addReg(MCS51::C, RegState::ImplicitDefine);
    if (PreserveA)
      BuildMI(MBB, I, DL, TII.get(MCS51::POP_DIRECT))
          .addImm(0xE0)
          .addReg(MCS51::A, RegState::ImplicitDefine);
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

  if (MI->getOpcode() == MCS51::SPILL_LOAD_IM) {
    EmitSpillAddress();
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IM_R1_IND),
            MI->getOperand(0).getReg());
    MI->eraseFromParent();
    return true;
  }
  if (MI->getOpcode() == MCS51::SPILL_STORE_IM) {
    EmitSpillAddress();
    Register Src = MI->getOperand(FIOperandNum + 2).getReg();
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_R1_IND_IM))
        .addReg(Src, ByteUndef(Src));
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
      if (Offset >= -4 && Offset <= 4) {
        EmitDirectStackAddress(Offset);
      } else {
        BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_PSW))
          .addReg(MCS51::C, RegState::Implicit);
        EmitAddressAtOffset(OffsetAfterPush(1));
        BuildMI(MBB, I, DL, TII.get(MCS51::POP_PSW))
          .addReg(MCS51::C, RegState::ImplicitDefine);
      }
    } else if (MI->getOpcode() == MCS51::LOAD_FRAME8 ||
               MI->getOpcode() == MCS51::LOAD_FRAME16) {
      EmitAddress();
    } else {
      EmitSpillAddress();
    }
    if ((MI->getOpcode() == MCS51::SPILL_LOAD16 ||
         MI->getOpcode() == MCS51::LOAD_FRAME16) &&
        Dst != MCS51::DPTR) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IM_R1_IND), PairLo(Dst));
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IM_R1_IND), PairHi(Dst));
    } else if (MI->getOpcode() == MCS51::SPILL_LOAD16) {
      UsesDirectWordTransfer = true;
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_R1_IND))
          .addImm(0x82)
          .addReg(MCS51::R1, RegState::Implicit);
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_R1_IND))
          .addImm(0x83)
          .addReg(MCS51::R1, RegState::Implicit);
    } else if (MI->getOpcode() == MCS51::SPILL_LOAD8 ||
               MI->getOpcode() == MCS51::SPILL_LOAD_INDIRECT8) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_R1_IND), Dst);
    } else {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R1);
    }
    if (MI->getOpcode() == MCS51::SPILL_LOAD_A8) {
      // The load itself leaves the byte in the accumulator register class.
    } else if (MI->getOpcode() == MCS51::SPILL_LOAD8 ||
               MI->getOpcode() == MCS51::SPILL_LOAD_INDIRECT8) {
      // Already loaded straight into the register.
    } else if (MI->getOpcode() == MCS51::LOAD_FRAME8) {
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), Dst);
    } else if (MI->getOpcode() != MCS51::SPILL_LOAD16 &&
               !(MI->getOpcode() == MCS51::LOAD_FRAME16 &&
                 Dst != MCS51::DPTR)) {
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
    if ((MI->getOpcode() == MCS51::SPILL_STORE16 ||
         MI->getOpcode() == MCS51::STORE_FRAME16) &&
        Src != MCS51::DPTR) {
      if (MI->getOpcode() == MCS51::SPILL_STORE16)
        EmitSpillAddress();
      else
        EmitAddress();
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_R1_IND_IM))
          .addReg(PairLo(Src), ByteUndef(PairLo(Src)));
      BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1)
          .addReg(MCS51::R1);
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_R1_IND_IM))
          .addReg(PairHi(Src), ByteUndef(PairHi(Src)));
    } else if (MI->getOpcode() == MCS51::SPILL_STORE16) {
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
      if (Offset >= -6 && Offset <= 6) {
        EmitDirectStackAddress(Offset);
      } else {
        // Save A and PSW while forming the address so the spill preserves
        // both the accumulator value and live condition flags.
        BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_DIRECT))
          .addImm(0xE0)
          .addReg(MCS51::A, RegState::Implicit);
        BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_PSW))
          .addReg(MCS51::C, RegState::Implicit);
        EmitAddressAtOffset(OffsetAfterPush(2));
        BuildMI(MBB, I, DL, TII.get(MCS51::POP_PSW))
          .addReg(MCS51::C, RegState::ImplicitDefine);
        BuildMI(MBB, I, DL, TII.get(MCS51::POP_DIRECT))
          .addImm(0xE0)
          .addReg(MCS51::A, RegState::ImplicitDefine);
      }
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IND_RI_A)).addReg(MCS51::R1);
    } else if (MI->getOpcode() == MCS51::SPILL_STORE8 ||
               MI->getOpcode() == MCS51::SPILL_STORE_INDIRECT8) {
      EmitSpillAddress();
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_R1_IND_RN)).addReg(Src);
    } else if (MI->getOpcode() == MCS51::STORE_FRAME8) {
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

// Objects moved to the function's static PDATA frame are reached with
// MOV R1,#frame+k and MOVX through A. A is saved around the access when the
// pseudo promises to preserve it; C, DPTR and SP are never touched.
bool MCS51RegisterInfo::eliminateStaticFrameIndex(
    MachineBasicBlock::iterator MI, unsigned FIOperandNum) const {
  MachineBasicBlock &MBB = *MI->getParent();
  MachineFunction &MF = *MBB.getParent();
  auto *FuncInfo = MF.getInfo<MCS51MachineFunctionInfo>();
  int FI = MI->getOperand(FIOperandNum).getIndex();
  const unsigned *Base = FuncInfo->getStaticFrameOffset(FI);
  if (!Base)
    return false;
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const DebugLoc &DL = MI->getDebugLoc();
  unsigned Opc = MI->getOpcode();
  int64_t Offset = *Base + MI->getOperand(FIOperandNum + 1).getImm();
  auto I = MI;
  auto PairLo = [&](Register Pair) { return getSubReg(Pair, MCS51::sub_lo); };
  auto PairHi = [&](Register Pair) { return getSubReg(Pair, MCS51::sub_hi); };
  auto ByteUndef = [&](Register Byte) {
    for (auto It = I; It != MBB.begin();) {
      --It;
      if (It->isDebugInstr())
        continue;
      if (It->modifiesRegister(Byte, this))
        return RegState{};
      if (It->killsRegister(Byte, this))
        return RegState::Undef;
    }
    for (const MachineBasicBlock::RegisterMaskPair &LI : MBB.liveins())
      if (regsOverlap(LI.PhysReg, Byte))
        return RegState{};
    return RegState::Undef;
  };
  auto IsLiveAfter = [&](MCRegister Reg) {
    for (auto It = std::next(MI); It != MBB.end(); ++It) {
      if (It->isDebugInstr())
        continue;
      if (It->readsRegister(Reg, this))
        return true;
      if (It->modifiesRegister(Reg, this))
        return false;
    }
    for (const MachineBasicBlock *Succ : MBB.successors())
      if (Succ->isLiveIn(Reg))
        return true;
    return false;
  };
  auto Address = [&](int64_t Extra) {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_IMM))
        .addReg(MCS51::R1, RegState::Define)
        .addGlobalAddress(FuncInfo->getStaticFrame(), Offset + Extra);
  };
  auto IncR1 = [&]() {
    BuildMI(MBB, I, DL, TII.get(MCS51::INC_RN), MCS51::R1).addReg(MCS51::R1);
  };
  auto LoadA = [&]() {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOVX_A_IND_RI)).addReg(MCS51::R1);
  };
  auto StoreA = [&]() {
    BuildMI(MBB, I, DL, TII.get(MCS51::MOVX_IND_RI_A)).addReg(MCS51::R1);
  };
  auto AToByte = [&](Register Dst) {
    if (Dst == MCS51::A)
      return;
    if (MCS51::MCS51Imag8RegClass.contains(Dst))
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_IM_A), Dst).addReg(MCS51::A);
    else if (Dst == MCS51::DPL || Dst == MCS51::DPH)
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_DIRECT_A))
          .addImm(Dst == MCS51::DPL ? 0x82 : 0x83);
    else
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_RN_A), Dst);
  };
  auto ByteToA = [&](Register Src, RegState Flags) {
    if (Src == MCS51::A)
      return;
    if (MCS51::MCS51Imag8RegClass.contains(Src))
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_IM), MCS51::A)
          .addReg(Src, Flags);
    else if (Src == MCS51::DPL || Src == MCS51::DPH)
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(Src == MCS51::DPL ? 0x82 : 0x83);
    else
      BuildMI(MBB, I, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src, Flags);
  };
  bool ClobbersA = Opc == MCS51::LOAD_FRAME8 || Opc == MCS51::STORE_FRAME8 ||
                   Opc == MCS51::LOAD_FRAME16 || Opc == MCS51::STORE_FRAME16;
  bool IsLoad = Opc == MCS51::SPILL_LOAD8 || Opc == MCS51::SPILL_LOAD_IM ||
                Opc == MCS51::SPILL_LOAD_INDIRECT8 ||
                Opc == MCS51::SPILL_LOAD16 || Opc == MCS51::SPILL_LOAD_A8 ||
                Opc == MCS51::LOAD_FRAME8 || Opc == MCS51::LOAD_FRAME16;
  Register Value = IsLoad ? MI->getOperand(0).getReg()
                          : MI->getOperand(FIOperandNum + 2).getReg();
  bool DefinesA = IsLoad && Value == MCS51::A;
  bool StoresA = !IsLoad && Value == MCS51::A;
  bool SaveA = !ClobbersA && !DefinesA && !StoresA && IsLiveAfter(MCS51::A);
  if (SaveA)
    BuildMI(MBB, I, DL, TII.get(MCS51::PUSH_DIRECT))
        .addImm(0xE0)
        .addReg(MCS51::A, RegState::Implicit);
  bool Word = Opc == MCS51::SPILL_LOAD16 || Opc == MCS51::SPILL_STORE16 ||
              Opc == MCS51::LOAD_FRAME16 || Opc == MCS51::STORE_FRAME16;
  if (Word) {
    Register Lo = Value == MCS51::DPTR ? Register(MCS51::DPL) : Register(PairLo(Value));
    Register Hi = Value == MCS51::DPTR ? Register(MCS51::DPH) : Register(PairHi(Value));
    Address(0);
    if (IsLoad) {
      LoadA();
      AToByte(Lo);
      IncR1();
      LoadA();
      AToByte(Hi);
    } else {
      ByteToA(Lo, Value == MCS51::DPTR ? RegState{} : ByteUndef(Lo));
      StoreA();
      IncR1();
      ByteToA(Hi, Value == MCS51::DPTR ? RegState{} : ByteUndef(Hi));
      StoreA();
    }
  } else if (IsLoad) {
    Address(0);
    LoadA();
    AToByte(Value);
  } else {
    ByteToA(Value, RegState{});
    Address(0);
    StoreA();
  }
  if (SaveA)
    BuildMI(MBB, I, DL, TII.get(MCS51::POP_DIRECT))
        .addImm(0xE0)
        .addReg(MCS51::A, RegState::ImplicitDefine);
  MF.getRegInfo().clearKillFlags(MCS51::A);
  MI->eraseFromParent();
  return true;
}

Register MCS51RegisterInfo::getFrameRegister(const MachineFunction &) const {
  return MCS51::SP;
}

const TargetRegisterClass *
MCS51RegisterInfo::getPointerRegClass(unsigned) const {
  return &MCS51::MCS51GPR16RegClass;
}
