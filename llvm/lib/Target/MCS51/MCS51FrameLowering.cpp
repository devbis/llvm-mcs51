#include "MCS51FrameLowering.h"
#include <set>
#include "MCS51InstrInfo.h"
#include "MCS51RegisterInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
#include "llvm/ADT/Twine.h"
#include "MCS51.h"
#include "MCS51MachineFunctionInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<bool> MCS51StaticFrames(
    "mcs51-static-frames", cl::Hidden, cl::init(false),
    cl::desc("move eligible fixed-size frame objects of non-reentrant "
             "functions into a static PDATA frame (experimental)"));

bool MCS51FrameLowering::isStaticFrameAccess(unsigned Opcode) {
  switch (Opcode) {
  case MCS51::SPILL_LOAD8:
  case MCS51::SPILL_STORE8:
  case MCS51::SPILL_LOAD_IM:
  case MCS51::SPILL_STORE_IM:
  case MCS51::SPILL_LOAD_INDIRECT8:
  case MCS51::SPILL_STORE_INDIRECT8:
  case MCS51::SPILL_LOAD16:
  case MCS51::SPILL_STORE16:
  case MCS51::SPILL_LOAD_A8:
  case MCS51::SPILL_STORE_A8:
  case MCS51::LOAD_FRAME8:
  case MCS51::STORE_FRAME8:
  case MCS51::LOAD_FRAME16:
  case MCS51::STORE_FRAME16:
    return true;
  default:
    // Address materialisation (FRAMEADDR*) and indexed accesses produce an
    // IDATA pointer to the object; such objects stay on the hardware stack.
    return false;
  }
}

// A static frame is valid only while the function cannot have two live
// activations. The compiler rejects what it can see locally; whole-program
// recursion, interrupt reachability and indirect calls are checked by the
// link-time frame allocator, which fails closed.
void MCS51FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *) const {
  if (!MCS51StaticFrames)
    return;
  const Function &F = MF.getFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (F.hasFnAttribute("interrupt") || F.isVarArg() ||
      F.hasFnAttribute(Attribute::Naked) ||
      F.hasFnAttribute(Attribute::ReturnsTwice) ||
      F.callsFunctionThatReturnsTwice() || MFI.hasVarSizedObjects() ||
      F.getName() == "main")
    return;
  for (const BasicBlock &BB : F)
    for (const Instruction &I : BB)
      if (const auto *CB = dyn_cast<CallBase>(&I))
        if (CB->getCalledFunction() == &F)
          return;

  int Begin = 0, End = MFI.getObjectIndexEnd();
  if (End <= Begin)
    return;
  SmallVector<bool, 32> Eligible(End, true);
  for (const MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : MBB)
      for (const MachineOperand &MO : MI.operands())
        if (MO.isFI() && MO.getIndex() >= 0 &&
            (MI.isDebugInstr() || !isStaticFrameAccess(MI.getOpcode())))
          Eligible[MO.getIndex()] = false;

  auto *FI = MF.getInfo<MCS51MachineFunctionInfo>();
  unsigned Size = 0;
  SmallVector<std::pair<int, unsigned>, 16> Moved;
  for (int I = Begin; I < End; ++I) {
    if (!Eligible[I] || MFI.isDeadObjectIndex(I) ||
        MFI.isVariableSizedObjectIndex(I) || MFI.getObjectSize(I) <= 0 ||
        MFI.getObjectSize(I) > 255 - Size)
      continue;
    Moved.push_back({I, Size});
    Size += MFI.getObjectSize(I);
  }
  if (Moved.empty())
    return;
  Module &M = *const_cast<Module *>(F.getParent());
  auto *Ty = ArrayType::get(Type::getInt8Ty(M.getContext()), Size);
  auto *GV = new GlobalVariable(
      M, Ty, false, GlobalValue::InternalLinkage, Constant::getNullValue(Ty),
      "__sframe." + F.getName(), nullptr, GlobalValue::NotThreadLocal,
      MCS51::PData);
  GV->setSection((".mcs51.pdata.bss.sframe." + F.getName()).str());
  GV->setAlignment(Align(1));
  GV->setDSOLocal(true);
  FI->setStaticFrame(GV);
  for (auto [Index, Offset] : Moved) {
    FI->setStaticFrameOffset(Index, Offset);
    MFI.RemoveStackObject(Index);
  }
}

MCS51FrameLowering::MCS51FrameLowering()
    : TargetFrameLowering(StackGrowsUp, Align(1), 1) {}

// Callee-saved registers fall into two groups that the runtime can save and
// restore with shared routines: R2.. and IM8... A group uses the routines
// when it is a prefix of its register range with at least three members.
enum { CSRGroupR, CSRGroupIM, NumCSRGroups };

static int getCSRGroup(const TargetRegisterInfo *TRI, MCRegister Reg,
                       unsigned &Index) {
  unsigned Enc = TRI->getEncodingValue(Reg);
  if (Enc >= 2 && Enc <= 7) {
    Index = Enc - 2;
    return CSRGroupR;
  }
  if (MCS51::MCS51Imag8RegClass.contains(Reg) && Enc >= 56 && Enc <= 71) {
    Index = Enc - 56;
    return CSRGroupIM;
  }
  return -1;
}

static MCRegister getCSRGroupReg(int Group, unsigned Index) {
  return Group == CSRGroupR ? MCRegister(MCS51::R2 + Index)
                            : MCRegister(MCS51::IM8 + Index);
}

// Number of registers of each group handled by the shared routines (0 if the
// group is saved inline).
static void getCSRHelperCounts(const TargetRegisterInfo *TRI,
                               ArrayRef<CalleeSavedInfo> CSI,
                               unsigned (&Counts)[NumCSRGroups]) {
  unsigned Members[NumCSRGroups] = {};
  unsigned Mask[NumCSRGroups] = {};
  for (const CalleeSavedInfo &CS : CSI) {
    unsigned Index;
    int Group = getCSRGroup(TRI, CS.getReg(), Index);
    if (Group < 0)
      continue;
    ++Members[Group];
    Mask[Group] |= 1u << Index;
  }
  for (int G = 0; G != NumCSRGroups; ++G) {
    bool Prefix = (Mask[G] & (Mask[G] + 1)) == 0;
    Counts[G] = Prefix && Members[G] >= 3 ? Members[G] : 0;
  }
}

struct CSRSaveAction {
  int Group;
  MCRegister Reg;
};

// Return the stack operations emitted by spillCalleeSavedRegisters, in order.
// A grouped helper saves its whole register prefix at the first occurrence of
// any member. The CSI list need not keep a group contiguous: the target's
// assignCalleeSavedSpillSlots can append registers added to round up a prefix.
static SmallVector<CSRSaveAction, 16>
getCSRSaveActions(const TargetRegisterInfo *TRI,
                  ArrayRef<CalleeSavedInfo> CSI,
                  const unsigned (&Counts)[NumCSRGroups]) {
  SmallVector<CSRSaveAction, 16> Actions;
  bool Called[NumCSRGroups] = {};
  for (const CalleeSavedInfo &CS : CSI) {
    unsigned Index;
    int Group = getCSRGroup(TRI, CS.getReg(), Index);
    if (Group >= 0 && Counts[Group]) {
      if (Called[Group])
        continue;
      Called[Group] = true;
      Actions.push_back({Group, MCRegister()});
      continue;
    }
    Actions.push_back({-1, CS.getReg()});
  }
  return Actions;
}

bool MCS51FrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  // Frame objects are plain bytes on the 8-bit stack. Source alignments of 2
  // for 16-bit objects would only add padding to the 255-byte stack window.
  MachineFrameInfo &MFI = MF.getFrameInfo();
  for (int I = 0, E = MFI.getObjectIndexEnd(); I < E; ++I)
    if (!MFI.isDeadObjectIndex(I))
      MFI.setObjectAlignment(I, Align(1));
  // Callee-saved R registers are pushed directly; they do not need frame slots.
  if (MF.getFunction().hasFnAttribute("interrupt"))
    return true;
  // Round a group up to a prefix of its range so that one shared routine can
  // save it, when only a couple of unused registers have to be added.
  unsigned Members[NumCSRGroups] = {};
  unsigned Top[NumCSRGroups] = {};
  for (const CalleeSavedInfo &CS : CSI) {
    unsigned Index;
    int Group = getCSRGroup(TRI, CS.getReg(), Index);
    if (Group < 0)
      continue;
    ++Members[Group];
    Top[Group] = std::max(Top[Group], Index + 1);
  }
  for (int G = 0; G != NumCSRGroups; ++G) {
    if (Members[G] < 3 || Top[G] - Members[G] > 2)
      continue;
    for (unsigned Index = 0; Index != Top[G]; ++Index) {
      MCRegister Reg = getCSRGroupReg(G, Index);
      if (llvm::none_of(CSI, [&](const CalleeSavedInfo &CS) {
            return CS.getReg() == Reg;
          }))
        CSI.push_back(CalleeSavedInfo(Reg));
    }
  }
  return true;
}

bool MCS51FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const TargetInstrInfo &TII = *MBB.getParent()->getSubtarget().getInstrInfo();
  unsigned Counts[NumCSRGroups];
  getCSRHelperCounts(TRI, CSI, Counts);
  for (const CalleeSavedInfo &CS : CSI)
    MBB.addLiveIn(CS.getReg());

  SmallVector<CSRSaveAction, 16> Actions =
      getCSRSaveActions(TRI, CSI, Counts);
  for (const CSRSaveAction &Action : Actions) {
    if (Action.Group >= 0) {
      int Group = Action.Group;
      auto MIB = BuildMI(MBB, MI, DebugLoc(), TII.get(MCS51::CSR_SAVE))
                     .addImm(Group)
                     .addImm(Counts[Group]);
      for (unsigned I = 0; I != Counts[Group]; ++I)
        MIB.addReg(getCSRGroupReg(Group, I), RegState::Implicit);
      MIB.setMIFlag(MachineInstr::FrameSetup);
      continue;
    }

    Register Reg = Action.Reg;
    unsigned Direct = TRI->getEncodingValue(Reg);
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
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const TargetInstrInfo &TII = *MBB.getParent()->getSubtarget().getInstrInfo();
  unsigned Counts[NumCSRGroups];
  getCSRHelperCounts(TRI, CSI, Counts);
  SmallVector<CSRSaveAction, 16> Actions =
      getCSRSaveActions(TRI, CSI, Counts);
  for (const CSRSaveAction &Action : llvm::reverse(Actions)) {
    if (Action.Group >= 0) {
      int Group = Action.Group;
      auto MIB = BuildMI(MBB, MI, DebugLoc(), TII.get(MCS51::CSR_RESTORE))
                     .addImm(Group)
                     .addImm(Counts[Group]);
      for (unsigned I = 0; I != Counts[Group]; ++I)
        MIB.addReg(getCSRGroupReg(Group, I), RegState::ImplicitDefine);
      MIB.setMIFlag(MachineInstr::FrameDestroy);
      continue;
    }
    Register Reg = Action.Reg;
    unsigned Direct = TRI->getEncodingValue(Reg);
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

static SmallVector<unsigned, 40>
getInterruptSaveAddresses(const MachineFunction &MF, bool Reverse) {
  // An interrupt may arrive with arbitrary values in the register bank, so
  // save every architectural register that the handler actually clobbers.
  // Calls carry their complete clobber list and therefore remain conservative.
  bool SavePSW = false, SaveA = false, SaveB = false;
  bool SaveDPL = false, SaveDPH = false;
  bool SaveR[8] = {};
  const auto &TRI = *MF.getSubtarget().getRegisterInfo();
  // Imaginary registers are DATA RAM bytes the interrupted code may be using.
  std::set<unsigned> SaveImag;
  auto Mark = [&](MCRegister Reg) {
    if (MCS51::MCS51Imag8RegClass.contains(Reg)) {
      SaveImag.insert(TRI.getEncodingValue(Reg));
      return;
    }
    if (MCS51::MCS51GPR16RegClass.contains(Reg)) {
      SaveImag.insert(TRI.getEncodingValue(TRI.getSubReg(Reg, MCS51::sub_lo)));
      SaveImag.insert(TRI.getEncodingValue(TRI.getSubReg(Reg, MCS51::sub_hi)));
      return;
    }
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
        else if (MO.isRegMask()) {
          // A reduced callee-saved set (-mcs51-csr) lets callees clobber
          // R2-R7 as well, which is only visible through the mask.
          for (MCRegister R : {MCS51::R0, MCS51::R1, MCS51::R2, MCS51::R3,
                               MCS51::R4, MCS51::R5, MCS51::R6, MCS51::R7})
            if (MO.clobbersPhysReg(R))
              Mark(R);
          for (unsigned Imag = MCS51::IM0; Imag <= MCS51::IM23; ++Imag)
            if (MO.clobbersPhysReg(Imag))
              Mark(Imag);
        }
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

  SmallVector<unsigned, 40> Addresses;
  if (SavePSW) Addresses.push_back(0xd0);
  if (SaveA) Addresses.push_back(0xe0);
  if (SaveB) Addresses.push_back(0xf0);
  if (SaveDPL) Addresses.push_back(0x82);
  if (SaveDPH) Addresses.push_back(0x83);
  for (unsigned I = 0; I != 8; ++I)
    if (SaveR[I]) Addresses.push_back(I);
  for (unsigned Address : SaveImag)
    Addresses.push_back(Address);
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
  // Frame-setup instructions load incoming stack arguments relative to the
  // entry SP, so the frame is allocated after the last of them. Copies that
  // the allocator placed ahead of some of them do not end the sequence.
  MachineBasicBlock::iterator I = MBB.begin();
  for (auto It = MBB.begin(); It != MBB.end(); ++It)
    if (It->getFlag(MachineInstr::FrameSetup))
      I = std::next(It);
  while (I != MBB.end() && I->isDebugInstr())
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
  // The frame lies above the pushed callee-saved registers, so it is
  // released before they are popped.
  auto Release = I;
  while (Release != MBB.begin() &&
         std::prev(Release)->getFlag(MachineInstr::FrameDestroy))
    --Release;
  emitStackAdjustment(MBB, Release, TII, StackSize, /*Deallocate=*/true,
                      PreserveA);
  if (MF.getFunction().hasFnAttribute("interrupt")) {
    // The prologue pushed the callee-saved registers before the interrupt
    // save set, so the interrupt registers must be popped before the
    // callee-saved restores that PEI placed directly ahead of the
    // terminator. Popping at the terminator instead inverts the order
    // and restores every saved slot from the wrong stack position.
    for (unsigned Address : getInterruptSaveAddresses(MF, true))
      BuildMI(MBB, Release, DebugLoc(), TII.get(MCS51::POP_DIRECT))
          .addImm(Address)
          .addReg(MCS51::SP, RegState::ImplicitDefine)
          .setMIFlag(MachineInstr::FrameDestroy);
  }
}

StackOffset MCS51FrameLowering::getFrameIndexReference(
    const MachineFunction &MF, int FI, Register &FrameReg) const {
  FrameReg = MCS51::SP;
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // Incoming stack arguments lie below the callee-saved registers that the
  // prologue pushes; locals lie above them.
  int64_t Pushed = FI < 0 ? static_cast<int64_t>(MFI.getCalleeSavedInfo().size())
                          : 0;
  return StackOffset::getFixed(MFI.getObjectOffset(FI) - MFI.getStackSize() -
                               Pushed);
}

MachineBasicBlock::iterator MCS51FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  // The pushes and pops of stack arguments are explicit instructions; the
  // pseudos only tell frame-index elimination how far SP has moved.
  return MBB.erase(MI);
}

bool MCS51FrameLowering::hasFPImpl(const MachineFunction &) const {
  return false;
}
