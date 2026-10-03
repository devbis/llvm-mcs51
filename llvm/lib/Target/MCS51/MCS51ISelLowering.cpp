#include "MCS51ISelLowering.h"
#include "MCS51.h"
#include "MCS51Banking.h"
#include "MCS51MachineFunctionInfo.h"
#include "MCS51SelectionDAGInfo.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "MCS51GenCallingConv.inc"

static bool isWordImmediate(const MachineInstr &MI) {
  return (MI.getOpcode() == MCS51::MOV_DPTR_IMM ||
          MI.getOpcode() == MCS51::LDI16) &&
         MI.getOperand(1).isImm();
}

// One byte of a 16-bit value. This is a plain subregister copy, so a value
// held in a register pair never has to visit DPTR to be split.
static Register extractWordByte(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, const TargetInstrInfo &TII,
                                MachineRegisterInfo &MRI, Register Word,
                                bool High) {
  Register Byte = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
  BuildMI(MBB, I, DL, TII.get(TargetOpcode::COPY), Byte)
      .addReg(Word, {}, High ? MCS51::sub_hi : MCS51::sub_lo);
  return Byte;
}

static void buildWord(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                      const DebugLoc &DL, const TargetInstrInfo &TII,
                      Register Dst, Register Lo, Register Hi) {
  BuildMI(MBB, I, DL, TII.get(TargetOpcode::REG_SEQUENCE), Dst)
      .addReg(Lo)
      .addImm(MCS51::sub_lo)
      .addReg(Hi)
      .addImm(MCS51::sub_hi);
}

// A byte held in an imaginary register. A pair built from two of these needs
// no copies once the allocator coalesces them into the pair's halves.
static Register saveAToImag(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator I, const DebugLoc &DL,
                            const TargetInstrInfo &TII,
                            MachineRegisterInfo &MRI) {
  Register Byte = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
  BuildMI(MBB, I, DL, TII.get(TargetOpcode::COPY), Byte).addReg(MCS51::A);
  return Byte;
}

// One byte of a word operand for a byte-wise compare: a half of a register
// pair, or a constant taken from the instruction that materialized the word.
struct WordByte {
  Register Reg;
  unsigned Sub = 0;
  bool IsImm = false;
  uint8_t Imm = 0;
};

static void describeWord(MachineRegisterInfo &MRI, Register Word,
                         SmallVectorImpl<WordByte> &Out) {
  MachineInstr *Def = MRI.getVRegDef(Word);
  if (Def && Def->getOpcode() == MCS51::LDI16 && Def->getOperand(1).isImm()) {
    uint16_t Value = Def->getOperand(1).getImm();
    Out.push_back({Register(), 0, true, uint8_t(Value & 0xff)});
    Out.push_back({Register(), 0, true, uint8_t(Value >> 8)});
    return;
  }
  Out.push_back({Word, MCS51::sub_lo, false, 0});
  Out.push_back({Word, MCS51::sub_hi, false, 0});
}

// Leaves the outcome of comparing L with R in the carry flag (kinds 0, 1, 3
// and 4: the borrow of L - R, with the sign bit of the top byte flipped for
// the signed kinds) or, for kind 2, zero in A when the operands are equal.
// The caller inverts as needed for the greater-or-equal kinds.
static void emitWordCompare(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MII,
                            const DebugLoc &DL, const TargetInstrInfo &TII,
                            ArrayRef<WordByte> L, ArrayRef<WordByte> R,
                            int64_t Kind) {
  unsigned Count = L.size();
  auto LoadA = [&](const WordByte &B) {
    if (B.IsImm)
      BuildMI(MBB, MII, DL, TII.get(MCS51::MOV_A_IMM), MCS51::A).addImm(B.Imm);
    else
      BuildMI(MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
          .addReg(B.Reg, RegState{}, B.Sub);
  };
  auto Alu = [&](unsigned ImOpcode, unsigned ImmOpcode, const WordByte &B) {
    if (B.IsImm)
      BuildMI(MBB, MII, DL, TII.get(ImmOpcode), MCS51::A).addImm(B.Imm);
    else
      BuildMI(MBB, MII, DL, TII.get(ImOpcode)).addReg(B.Reg, RegState{}, B.Sub);
  };
  if (Kind == 2 || Kind == 5) {
    // XOR the bytes pairwise and OR the differences; A ends up zero when all
    // match. A zero constant makes the XOR a plain load of the other byte.
    bool First = true;
    for (unsigned I = 0; I != Count; ++I) {
      bool RZero = R[I].IsImm && R[I].Imm == 0;
      bool LZero = L[I].IsImm && L[I].Imm == 0;
      if (LZero && RZero)
        continue;
      const WordByte &Byte = LZero ? R[I] : L[I];
      const WordByte *Other = (LZero || RZero) ? nullptr : &R[I];
      if (First) {
        LoadA(Byte);
        if (Other)
          Alu(MCS51::XRL_A_IM, MCS51::XRL_A_IMM, *Other);
        First = false;
        continue;
      }
      if (!Other) {
        // OR the byte straight into the accumulated difference.
        Alu(MCS51::ORL_A_IM, MCS51::ORL_A_IMM, Byte);
        continue;
      }
      BuildMI(MBB, MII, DL, TII.get(MCS51::MOV_B_A));
      LoadA(Byte);
      Alu(MCS51::XRL_A_IM, MCS51::XRL_A_IMM, *Other);
      BuildMI(MBB, MII, DL, TII.get(MCS51::ORL_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    }
    if (First)
      BuildMI(MBB, MII, DL, TII.get(MCS51::CLR_A));
    return;
  }
  bool IsSigned = Kind == 1 || Kind == 4;
  BuildMI(MBB, MII, DL, TII.get(MCS51::CLR_C));
  for (unsigned I = 0; I != Count; ++I) {
    bool Bias = IsSigned && I + 1 == Count;
    if (!Bias) {
      LoadA(L[I]);
      Alu(MCS51::SUBB_A_IM, MCS51::SUBB_A_IMM, R[I]);
      continue;
    }
    // Flip the sign bits so the borrow of an unsigned subtraction orders the
    // operands as signed values.
    if (R[I].IsImm) {
      LoadA(L[I]);
      BuildMI(MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A).addImm(0x80);
      BuildMI(MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A)
          .addImm(R[I].Imm ^ 0x80);
    } else {
      LoadA(R[I]);
      BuildMI(MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A).addImm(0x80);
      BuildMI(MBB, MII, DL, TII.get(MCS51::MOV_B_A));
      LoadA(L[I]);
      BuildMI(MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A).addImm(0x80);
      BuildMI(MBB, MII, DL, TII.get(MCS51::SUBB_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    }
  }
}

// The instruction that uses a word only as its XDATA or CODE address, if there
// is exactly one such user in the block. A word built just to address memory
// can be computed straight into DPTR.
static MachineInstr *singleDptrAddressUser(MachineRegisterInfo &MRI,
                                           Register Word,
                                           MachineBasicBlock *MBB) {
  if (!MRI.hasOneNonDBGUse(Word))
    return nullptr;
  MachineInstr &User = *MRI.use_nodbg_begin(Word)->getParent();
  if (User.getParent() != MBB)
    return nullptr;
  unsigned AddressOperand;
  switch (User.getOpcode()) {
  case MCS51::LOADX8:
  case MCS51::LOADX16:
  case MCS51::LOADCODE8:
  case MCS51::LOADCODE16:
    AddressOperand = 1;
    break;
  case MCS51::STOREX8:
  case MCS51::STOREX16:
    AddressOperand = 0;
    break;
  default:
    return nullptr;
  }
  if (!User.getOperand(AddressOperand).isReg() ||
      User.getOperand(AddressOperand).getReg() != Word)
    return nullptr;
  // The word must not also be the stored value.
  for (unsigned I = 0, E = User.getNumOperands(); I != E; ++I)
    if (I != AddressOperand && User.getOperand(I).isReg() &&
        User.getOperand(I).getReg() == Word)
      return nullptr;
  return &User;
}

// Points the user's address operand at DPTR itself, which the preceding
// instructions have already loaded.
static void markAddressInDptr(MachineInstr &User) {
  unsigned AddressOperand =
      (User.getOpcode() == MCS51::STOREX8 || User.getOpcode() == MCS51::STOREX16)
          ? 0
          : 1;
  User.getOperand(AddressOperand).setReg(MCS51::DPTR);
}

// Loads DPTR with Base + Offset in front of At. When the DPTR value already
// held in front of At is Base plus a smaller or equal offset, only increments
// are emitted; the 16-bit accesses that step DPTR are accounted for.
static void emitDptrAddress(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator At, const DebugLoc &DL,
                            const TargetInstrInfo &TII,
                            const TargetRegisterInfo &TRI,
                            MCS51MachineFunctionInfo &FuncInfo, Register Base,
                            uint16_t Offset) {
  // Find the nearest instruction that set DPTR and count the increments
  // after it. Scanning continues into the only predecessor of the first block
  // of an IR block whose single IR predecessor is that block.
  int64_t Increments = 0;
  const MachineInstr *Def = nullptr;
  MachineBasicBlock *Block = &MBB;
  auto It = At;
  for (unsigned Hops = 0; Hops != 4; ++Hops) {
    while (It != Block->begin()) {
      --It;
      if (It->isDebugInstr())
        continue;
      if (It->getOpcode() == MCS51::INC_DPTR) {
        ++Increments;
        continue;
      }
      if (It->isCall() || It->modifiesRegister(MCS51::DPTR, &TRI) ||
          It->isInlineAsm()) {
        Def = &*It;
        break;
      }
    }
    if (Def)
      break;
    const BasicBlock *IRBlock = Block->getBasicBlock();
    if (Block->pred_size() != 1 || !IRBlock)
      break;
    MachineBasicBlock *Pred = *Block->pred_begin();
    if (Pred == Block || !Pred->getBasicBlock() ||
        Pred->getBasicBlock() == IRBlock ||
        IRBlock->getSinglePredecessor() != Pred->getBasicBlock())
      break;
    Block = Pred;
    It = Block->end();
  }
  if (Def) {
    if (const auto *Known = FuncInfo.getDptrAddress(Def)) {
      int64_t Current = Known->second + Increments;
      int64_t Delta = int64_t(Offset) - Current;
      if (Known->first == Base && Delta >= 0 && Delta <= 6) {
        for (int64_t I = 0; I != Delta; ++I)
          BuildMI(MBB, At, DL, TII.get(MCS51::INC_DPTR));
        return;
      }
    }
  }
  MachineInstr *Last;
  if (Offset <= 6) {
    Last = BuildMI(MBB, At, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
               .addReg(Base)
               .getInstr();
    FuncInfo.setDptrAddress(Last, Base, 0);
    for (unsigned I = 0; I != Offset; ++I)
      BuildMI(MBB, At, DL, TII.get(MCS51::INC_DPTR));
    return;
  }
  auto Half = [&](unsigned Dest, unsigned Sub, bool Carry, uint8_t K) {
    BuildMI(MBB, At, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Base, RegState{}, Sub);
    BuildMI(MBB, At, DL,
            TII.get(Carry ? MCS51::ADDC_A_IMM : MCS51::ADD_A_IMM), MCS51::A)
        .addImm(K);
    return BuildMI(MBB, At, DL, TII.get(TargetOpcode::COPY), Dest)
        .addReg(MCS51::A)
        .getInstr();
  };
  if ((Offset & 0xff) == 0) {
    BuildMI(MBB, At, DL, TII.get(TargetOpcode::COPY), MCS51::DPL)
        .addReg(Base, RegState{}, MCS51::sub_lo);
    Last = Half(MCS51::DPH, MCS51::sub_hi, false, Offset >> 8);
  } else {
    Half(MCS51::DPL, MCS51::sub_lo, false, Offset & 0xff);
    Last = Half(MCS51::DPH, MCS51::sub_hi, true, Offset >> 8);
  }
  FuncInfo.setDptrAddress(Last, Base, Offset);
}

static bool containsFrameIndex(SDValue V) {
  SmallVector<SDValue, 8> Worklist(1, V);
  while (!Worklist.empty()) {
    SDValue Current = Worklist.pop_back_val();
    if (Current.getOpcode() == ISD::FrameIndex)
      return true;
    if (Current.getOpcode() == ISD::ADD ||
        Current.getOpcode() == ISD::BITCAST ||
        Current.getOpcode() == ISD::ADDRSPACECAST)
      for (SDValue Operand : Current->ops())
        Worklist.push_back(Operand);
  }
  return false;
}

// IDATA/PDATA globals without explicit sections are emitted in module order
// into one target section. Use their section-relative offsets to reuse R0 only
// when both symbols are local to this object and their relative layout is known.
static bool getMCS51IndirectGlobalSectionOffset(const GlobalValue *Value,
                                               unsigned AddressSpace,
                                               int64_t Addend,
                                               int64_t &Offset) {
  const auto *Target = dyn_cast<GlobalVariable>(Value);
  if (!Target || Target->getAddressSpace() != AddressSpace ||
      (AddressSpace != MCS51::IData && AddressSpace != MCS51::PData) ||
      Target->hasSection() || Target->isDeclaration() ||
      Target->isConstant() || Target->isThreadLocal() ||
      Target->hasCommonLinkage() || !Target->isDSOLocal())
    return false;

  const bool IsBSS = Target->getInitializer()->isNullValue();
  const Module &M = *Target->getParent();
  const DataLayout &DL = M.getDataLayout();
  uint64_t SectionOffset = 0;
  for (const GlobalVariable &GV : M.globals()) {
    if (GV.getAddressSpace() != AddressSpace || GV.hasSection() ||
        GV.isDeclaration() || GV.isConstant() || GV.isThreadLocal() ||
        GV.hasCommonLinkage() || !GV.hasInitializer() ||
        GV.getInitializer()->isNullValue() != IsBSS)
      continue;

    Align Alignment = DL.getPreferredAlign(&GV);
    if (MaybeAlign ExplicitAlignment = GV.getAlign())
      Alignment = std::max(Alignment, *ExplicitAlignment);
    SectionOffset = alignTo(SectionOffset, Alignment);
    if (&GV == Target) {
      int64_t Result = static_cast<int64_t>(SectionOffset) + Addend;
      if (Result < 0)
        return false;
      Offset = Result;
      return true;
    }

    TypeSize Size = DL.getTypeAllocSize(GV.getValueType());
    if (Size.isScalable())
      return false;
    SectionOffset += Size.getFixedValue();
  }
  return false;
}

static void emitIndirectGlobalAddress(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator InsertPt,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII,
                                      const TargetRegisterInfo *TRI,
                                      const MachineOperand &Address,
                                      unsigned AddressSpace) {
  int64_t TargetOffset;
  if (Address.isGlobal() &&
      getMCS51IndirectGlobalSectionOffset(Address.getGlobal(), AddressSpace,
                                          Address.getOffset(), TargetOffset)) {
    int64_t Delta = 0;
    bool Known = false;
    for (auto I = InsertPt; I != MBB.begin();) {
      --I;
      if (I->isDebugInstr())
        continue;
      if (!I->modifiesRegister(MCS51::R0, TRI))
        continue;

      if ((I->getOpcode() == MCS51::INC_RN ||
           I->getOpcode() == MCS51::DEC_RN) &&
          I->getOperand(0).getReg() == MCS51::R0 &&
          I->getOperand(1).getReg() == MCS51::R0) {
        Delta += I->getOpcode() == MCS51::INC_RN ? 1 : -1;
        continue;
      }

      if (I->getOpcode() == MCS51::MOV_RN_IMM &&
          I->getOperand(0).getReg() == MCS51::R0 &&
          I->getOperand(1).isGlobal()) {
        int64_t BaseOffset;
        if (getMCS51IndirectGlobalSectionOffset(
                I->getOperand(1).getGlobal(), AddressSpace,
                I->getOperand(1).getOffset(), BaseOffset)) {
          Known = true;
          Delta += BaseOffset;
        }
      }
      break;
    }

    if (Known) {
      int64_t Difference = TargetOffset - Delta;
      if (Difference == 0)
        return;
      if (Difference == 1 || Difference == -1) {
        BuildMI(MBB, InsertPt, DL,
                TII.get(Difference > 0 ? MCS51::INC_RN : MCS51::DEC_RN))
            .addReg(MCS51::R0, RegState::Define)
            .addReg(MCS51::R0);
        return;
      }
    }
  }

  BuildMI(MBB, InsertPt, DL, TII.get(MCS51::MOV_RN_IMM))
      .addReg(MCS51::R0, RegState::Define)
      .add(Address);
}

MCS51TargetLowering::MCS51TargetLowering(const TargetMachine &TM,
                                         const MCS51Subtarget &STI)
    : TargetLowering(TM, STI), STI(STI) {
  addRegisterClass(MVT::i8, &MCS51::MCS51GPR8RegClass);
  addRegisterClass(MVT::i16, &MCS51::MCS51GPR16RegClass);
  setOperationAction(ISD::UDIV, MVT::i8, Legal);
  setOperationAction(ISD::UREM, MVT::i8, Legal);
  setOperationAction(ISD::UDIV, MVT::i16, LibCall);
  setOperationAction(ISD::UREM, MVT::i16, LibCall);
  setOperationAction(ISD::SDIV, MVT::i16, LibCall);
  setOperationAction(ISD::SREM, MVT::i16, LibCall);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::ANY_EXTEND, MVT::i16, Custom);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Custom);
  // The 8051 has no indirect branch instruction; avoid jump-table lowering.
  setMinimumJumpTableEntries(~0U);
  setOperationAction(ISD::ADD, MVT::i16, Custom);
  setTargetDAGCombine(ISD::SUB);
  setTargetDAGCombine(ISD::SHL);
  setTargetDAGCombine(ISD::SRL);
  setTargetDAGCombine(ISD::SRA);
  setTargetDAGCombine(ISD::BRCOND);
  setTargetDAGCombine(ISD::TRUNCATE);
  setTargetDAGCombine(ISD::STORE);
  setOperationAction(ISD::SHL, MVT::i8, Legal);
  setOperationAction(ISD::SHL, MVT::i16, Custom);
  setOperationAction(ISD::SRL, MVT::i8, Legal);
  setOperationAction(ISD::ROTL, MVT::i8, Expand);
  setOperationAction(ISD::ROTL, MVT::i16, Expand);
  setOperationAction(ISD::ROTR, MVT::i8, Expand);
  setOperationAction(ISD::ROTR, MVT::i16, Expand);
  setOperationAction(ISD::SRA, MVT::i8, Custom);
  setOperationAction(ISD::SRA, MVT::i16, Custom);
  setOperationAction(ISD::SRL, MVT::i16, Custom);
  setOperationAction(ISD::FADD, MVT::f32, LibCall);
  setOperationAction(ISD::FSUB, MVT::f32, LibCall);
  setOperationAction(ISD::FMUL, MVT::f32, LibCall);
  setOperationAction(ISD::FDIV, MVT::f32, LibCall);
  setOperationAction(ISD::FP_TO_SINT, MVT::i64, LibCall);
  setOperationAction(ISD::FP_TO_UINT, MVT::i64, LibCall);
  setOperationAction(ISD::SINT_TO_FP, MVT::i64, LibCall);
  setOperationAction(ISD::UINT_TO_FP, MVT::i64, LibCall);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::MUL, MVT::i16, Custom);
  // Variable 32-bit shifts use the runtime helpers.
  setOperationAction(ISD::SHL_PARTS, MVT::i16, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i16, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i16, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i16, Expand);
  setOperationAction(ISD::MULHU, MVT::i16, Expand);
  setOperationAction(ISD::ADD, MVT::i32, Custom);
  setOperationAction(ISD::SUB, MVT::i32, Custom);
  setOperationAction(ISD::SETCC, MVT::i8, Custom);
  setOperationAction(ISD::SETCC, MVT::i16, Custom);
  setOperationAction(ISD::SETCC, MVT::i32, Custom);
  setOperationAction(ISD::SELECT, MVT::i8, Custom);
  setOperationAction(ISD::SELECT, MVT::i16, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i8, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i16, Custom);
  setOperationAction(ISD::BR_CC, MVT::i8, Custom);
  setOperationAction(ISD::BR_CC, MVT::i16, Custom);
  setBooleanContents(ZeroOrOneBooleanContent);
  // Route copies through MCS51SelectionDAGInfo so stack-space casts retain
  // their IDATA address space during inline lowering.
  MaxStoresPerMemcpy = MaxStoresPerMemcpyOptSize = 0;
  setStackPointerRegisterToSaveRestore(MCS51::SP);
  computeRegisterProperties(STI.getRegisterInfo());
}

EVT MCS51TargetLowering::getSetCCResultType(const DataLayout &, LLVMContext &,
                                            EVT VT) const {
  assert(!VT.isVector() && "MCS-51 does not support vector comparisons");
  return MVT::i8;
}

std::pair<unsigned, const TargetRegisterClass *>
MCS51TargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'a':
      if (VT == MVT::i8)
        return {0, &MCS51::MCS51ARegRegClass};
      break;
    case 'd':
      if (VT == MVT::i16)
        return {0, &MCS51::MCS51GPR16RegClass};
      break;
    case 'r':
      if (VT == MVT::i8)
        return {0, &MCS51::MCS51GPR8RegClass};
      if (VT == MVT::i16)
        return {0, &MCS51::MCS51GPR16RegClass};
      break;
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

TargetLowering::ConstraintType
MCS51TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1 &&
      (Constraint[0] == 'a' || Constraint[0] == 'd' ||
       Constraint[0] == 'r'))
    return C_RegisterClass;
  return TargetLowering::getConstraintType(Constraint);
}

TargetLowering::ShiftLegalizationStrategy
MCS51TargetLowering::preferredShiftLegalizationStrategy(
    SelectionDAG &DAG, SDNode *N, unsigned ExpansionFactor) const {
  if (N->getValueType(0) == MVT::i64)
    return ShiftLegalizationStrategy::LowerToLibcall;
  return TargetLowering::preferredShiftLegalizationStrategy(
      DAG, N, ExpansionFactor);
}

void MCS51TargetLowering::ReplaceNodeResults(
    SDNode *N, SmallVectorImpl<SDValue> &Results, SelectionDAG &DAG) const {
  if ((N->getOpcode() == ISD::ADD || N->getOpcode() == ISD::SUB) &&
      N->getValueType(0) == MVT::i32) {
    SDLoc DL(N);
    SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
    SDValue One = DAG.getConstant(1, DL, MVT::i16);
    SDValue LHSLo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                N->getOperand(0), Zero);
    SDValue LHSHi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                N->getOperand(0), One);
    SDValue RHSLo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                N->getOperand(1), Zero);
    SDValue RHSHi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                N->getOperand(1), One);
    {
      SDVTList ResultVTs = DAG.getVTList(MVT::i8, MVT::i8, MVT::i8, MVT::i8);
      unsigned Opcode = N->getOpcode() == ISD::ADD
                            ? MCS51ISD::ADD32_BYTES
                            : MCS51ISD::SUB32_BYTES;
      SDValue Sum = DAG.getNode(Opcode, DL, ResultVTs, LHSLo, LHSHi, RHSLo,
                                RHSHi);
      SDValue SumLo = DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i16, Sum,
                                  Sum.getValue(1));
      SDValue SumHi = DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i16,
                                  Sum.getValue(2), Sum.getValue(3));
      Results.push_back(DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, SumLo,
                                    SumHi));
    }
    return;
  }
  llvm_unreachable("unexpected MCS-51 operation with illegal result type");
}

SDValue MCS51TargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  if (Op.getOpcode() == ISD::SIGN_EXTEND_INREG &&
      Op.getValueType() == MVT::i16) {
    EVT FromVT = cast<VTSDNode>(Op.getOperand(1))->getVT();
    if (FromVT == MVT::i16)
      return Op.getOperand(0);
    unsigned Shift = 16 - FromVT.getSizeInBits();
    SDValue Amount = DAG.getConstant(Shift, DL, MVT::i16);
    SDValue Shifted = DAG.getNode(ISD::SHL, DL, MVT::i16, Op.getOperand(0),
                                  Amount);
    return DAG.getNode(ISD::SRA, DL, MVT::i16, Shifted, Amount);
  }
  if (Op.getOpcode() == ISD::SRA && Op.getValueType() == MVT::i8) {
    SDValue Amount = Op.getOperand(1);
    if (auto *C = dyn_cast<ConstantSDNode>(Amount)) {
      uint64_t Shift = std::min<uint64_t>(C->getZExtValue(), 8);
      return DAG.getNode(MCS51ISD::SRA8_IMM, DL, MVT::i8, Op.getOperand(0),
                         DAG.getConstant(Shift, DL, MVT::i16));
    }
    if (Amount.getValueType() != MVT::i16)
      Amount = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, Amount);
    return DAG.getNode(MCS51ISD::SRA8, DL, MVT::i8, Op.getOperand(0),
                       Amount);
  }
  if (Op.getOpcode() == ISD::VASTART)
    return LowerVASTART(Op, DAG);
  if (Op.getOpcode() == ISD::VAARG)
    return LowerVAARG(Op, DAG);

  if (Op.getOpcode() == ISD::ADD && Op.getValueType() == MVT::i16) {
    for (unsigned I = 0; I != 2; ++I) {
      SDValue Extended = Op.getOperand(I);
      SDValue Base = Op.getOperand(1 - I);

      bool IsSigned = false;
      SDValue Byte;
      LoadSDNode *ExtendedLoad = nullptr;
      if (auto *Load = dyn_cast<LoadSDNode>(Extended)) {
        if (Load->getMemoryVT() != MVT::i8 ||
            Load->getExtensionType() == ISD::NON_EXTLOAD ||
            !Extended.hasOneUse())
          continue;
        IsSigned = Load->getExtensionType() == ISD::SEXTLOAD;
        ExtendedLoad = Load;
        Byte = DAG.getLoad(MVT::i8, DL, Load->getChain(),
                           Load->getBasePtr(), Load->getMemOperand());
      } else if ((Extended.getOpcode() == ISD::SIGN_EXTEND ||
                  Extended.getOpcode() == ISD::ZERO_EXTEND) &&
                 Extended.getOperand(0).getValueType() == MVT::i8) {
        IsSigned = Extended.getOpcode() == ISD::SIGN_EXTEND;
        Byte = Extended.getOperand(0);
      }

      if (!Byte)
        continue;
      if (auto *Constant = dyn_cast<ConstantSDNode>(Base)) {
        if (ExtendedLoad)
          DAG.ReplaceAllUsesOfValueWith(SDValue(ExtendedLoad, 1),
                                        Byte.getValue(1));
        unsigned Opcode = IsSigned ? MCS51ISD::ADD_SEXT8_IMM
                                   : MCS51ISD::ADD_ZEXT8_IMM;
        return DAG.getNode(Opcode, DL, MVT::i16, Byte,
                           DAG.getConstant(Constant->getZExtValue(), DL,
                                           MVT::i16));
      }
      if (Base.getValueType() != MVT::i16 || containsFrameIndex(Base))
        continue;
      if (ExtendedLoad)
        DAG.ReplaceAllUsesOfValueWith(SDValue(ExtendedLoad, 1),
                                      Byte.getValue(1));
      unsigned Opcode = IsSigned ? MCS51ISD::ADD_SEXT8_16
                                 : MCS51ISD::ADD_ZEXT8_16;
      return DAG.getNode(Opcode, DL, MVT::i16, Base, Byte);
    }
    for (unsigned I = 0; I != 2; ++I) {
      SDValue Constant = Op.getOperand(I);
      SDValue Base = Op.getOperand(1 - I);
      if (auto *C = dyn_cast<ConstantSDNode>(Constant)) {
        // Leave stack and address-space arithmetic to the normal lowering.
        if (containsFrameIndex(Base) ||
            Base.getOpcode() == ISD::ADDRSPACECAST ||
            Base.getOpcode() == ISD::GlobalAddress)
          continue;
        uint16_t Immediate = C->getZExtValue();
        if (!Immediate)
          return Base;
        if (Immediate == 0xffff)
          return DAG.getNode(MCS51ISD::SUB16_DEC, DL, MVT::i16, Base);
        return DAG.getNode(MCS51ISD::ADD16_IMM, DL, MVT::i16, Base,
                           DAG.getConstant(Immediate, DL, MVT::i16));
      }
    }
    return SDValue();
  }

  if (Op.getOpcode() == ISD::ANY_EXTEND && Op.getValueType() == MVT::i16 &&
      Op.getOperand(0).getValueType() == MVT::i8) {
    // ANY_EXTEND leaves the high bits undefined, so zero is a valid choice.
    return DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, Op.getOperand(0));
  }
  auto LowerByteCompare = [&](SDValue LHS, SDValue RHS,
                              ISD::CondCode CC) -> SDValue {
    auto CompareUnsignedConstant = [&](SDValue Value, uint8_t Limit,
                                      bool IsLess) {
      unsigned Opcode = IsLess ? MCS51ISD::CMPULT8 : MCS51ISD::CMPUGE8;
      return DAG.getNode(Opcode, DL, MVT::i8, Value,
                         DAG.getConstant(Limit, DL, MVT::i8));
    };
    if (auto *C = dyn_cast<ConstantSDNode>(RHS)) {
      uint8_t Value = C->getZExtValue();
      switch (CC) {
      case ISD::SETULT:
        return CompareUnsignedConstant(LHS, Value, /*IsLess=*/true);
      case ISD::SETUGE:
        return CompareUnsignedConstant(LHS, Value, /*IsLess=*/false);
      case ISD::SETULE:
        if (Value == 0xff)
          return DAG.getConstant(1, DL, MVT::i8);
        return CompareUnsignedConstant(LHS, Value + 1, /*IsLess=*/true);
      case ISD::SETUGT:
        if (Value == 0xff)
          return DAG.getConstant(0, DL, MVT::i8);
        return CompareUnsignedConstant(LHS, Value + 1, /*IsLess=*/false);
      default:
        break;
      }
    }
    if (auto *C = dyn_cast<ConstantSDNode>(LHS)) {
      uint8_t Value = C->getZExtValue();
      switch (CC) {
      case ISD::SETULT:
        if (Value == 0xff)
          return DAG.getConstant(0, DL, MVT::i8);
        return CompareUnsignedConstant(RHS, Value + 1, /*IsLess=*/false);
      case ISD::SETUGE:
        if (Value == 0xff)
          return DAG.getConstant(1, DL, MVT::i8);
        return CompareUnsignedConstant(RHS, Value + 1, /*IsLess=*/true);
      case ISD::SETULE:
        return CompareUnsignedConstant(RHS, Value, /*IsLess=*/false);
      case ISD::SETUGT:
        return CompareUnsignedConstant(RHS, Value, /*IsLess=*/true);
      default:
        break;
      }
    }
    bool Invert = false;
    bool IsGreaterEqual = false;
    int64_t CompareKind = ISD::isSignedIntSetCC(CC) ? 1 : 0;
    switch (CC) {
    case ISD::SETEQ:
      CompareKind = 2;
      break;
    case ISD::SETNE:
      CompareKind = 2;
      Invert = true;
      break;
    case ISD::SETULT:
    case ISD::SETLT:
      break;
    case ISD::SETUGE:
    case ISD::SETGE:
      IsGreaterEqual = true;
      break;
    case ISD::SETUGT:
    case ISD::SETGT:
      std::swap(LHS, RHS);
      break;
    case ISD::SETULE:
    case ISD::SETLE:
      std::swap(LHS, RHS);
      IsGreaterEqual = true;
      break;
    default:
      return SDValue();
    }
    unsigned Opcode = CompareKind == 2
                          ? MCS51ISD::CMPEQ8
                          : CompareKind == 1
                                ? (IsGreaterEqual ? MCS51ISD::CMPSGE8
                                                  : MCS51ISD::CMPSLT8)
                                : (IsGreaterEqual ? MCS51ISD::CMPUGE8
                                                  : MCS51ISD::CMPULT8);
    SDValue Result = DAG.getNode(Opcode, DL, MVT::i8, LHS, RHS);
    return Invert ? DAG.getNode(ISD::XOR, DL, MVT::i8, Result,
                                DAG.getConstant(1, DL, MVT::i8))
                  : Result;
  };

  auto LowerWordCompare = [&](SDValue LHS, SDValue RHS, ISD::CondCode CC,
                              bool IsSigned) -> SDValue {
    bool Invert = false;
    bool IsGreaterEqual = false;
    switch (CC) {
    case ISD::SETEQ:
      return DAG.getNode(MCS51ISD::CMPEQ16, DL, MVT::i8, LHS, RHS);
    case ISD::SETNE:
      Invert = true;
      break;
    case ISD::SETULT:
    case ISD::SETLT:
      break;
    case ISD::SETUGE:
    case ISD::SETGE:
      IsGreaterEqual = true;
      break;
    case ISD::SETUGT:
    case ISD::SETGT:
      std::swap(LHS, RHS);
      break;
    case ISD::SETULE:
    case ISD::SETLE:
      std::swap(LHS, RHS);
      IsGreaterEqual = true;
      break;
    default:
      return SDValue();
    }
    unsigned Opcode = CC == ISD::SETNE
                          ? MCS51ISD::CMPEQ16
                          : IsSigned
                                ? (IsGreaterEqual ? MCS51ISD::CMPSGE16
                                                  : MCS51ISD::CMPSLT16)
                                : (IsGreaterEqual ? MCS51ISD::CMPUGE16
                                                  : MCS51ISD::CMPULT16);
    SDValue Result = DAG.getNode(Opcode, DL, MVT::i8, LHS, RHS);
    return Invert ? DAG.getNode(ISD::XOR, DL, MVT::i8, Result,
                                DAG.getConstant(1, DL, MVT::i8))
                  : Result;
  };
  if ((Op.getOpcode() == ISD::SRA || Op.getOpcode() == ISD::SRL ||
       Op.getOpcode() == ISD::SHL) &&
      Op.getValueType() == MVT::i16) {
    if (Op.getOpcode() == ISD::SHL && Op.getNode()->hasOneUse() &&
        (Op.getOperand(0).getOpcode() == ISD::SIGN_EXTEND ||
         Op.getOperand(0).getOpcode() == ISD::ZERO_EXTEND) &&
        Op.getOperand(0).getOperand(0).getValueType() == MVT::i8) {
      SDNode *User = Op.getNode()->use_begin()->getUser();
      if (User->getOpcode() == ISD::TRUNCATE &&
          User->getValueType(0) == MVT::i8 &&
          User->getOperand(0) == Op)
        return DAG.getNode(
            ISD::ZERO_EXTEND, DL, MVT::i16,
            DAG.getNode(MCS51ISD::SHL8, DL, MVT::i8,
                        Op.getOperand(0).getOperand(0), Op.getOperand(1)));
    }
    auto *Amount = dyn_cast<ConstantSDNode>(Op.getOperand(1));
    if (Op.getOpcode() != ISD::SRA && Amount &&
        Amount->getZExtValue() == 8) {
      SDValue Source = Op.getOperand(0);
      unsigned Opcode = Op.getOpcode() == ISD::SHL ? MCS51ISD::SHL16_8
                                                   : MCS51ISD::SRL16_8;
      return DAG.getNode(Opcode, DL, MVT::i16, Source);
    }
    unsigned Opcode = Op.getOpcode() == ISD::SHL
                          ? MCS51ISD::SHL16
                          : Op.getOpcode() == ISD::SRA ? MCS51ISD::SRA16
                                                       : MCS51ISD::SRL16;
    return DAG.getNode(Opcode, DL, MVT::i16, Op.getOperand(0),
                       Op.getOperand(1));
  }
  if (Op.getOpcode() == ISD::MUL && Op.getValueType() == MVT::i16) {
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);
    if (LHS.getOpcode() == ISD::ZERO_EXTEND &&
        LHS.getOperand(0).getValueType() == MVT::i8 &&
        RHS.getOpcode() == ISD::ZERO_EXTEND &&
        RHS.getOperand(0).getValueType() == MVT::i8)
      return DAG.getNode(MCS51ISD::MUL8_TO_16, DL, MVT::i16,
                         LHS.getOperand(0), RHS.getOperand(0));
    return DAG.getNode(MCS51ISD::MUL16, DL, MVT::i16, Op.getOperand(0),
                       Op.getOperand(1));
  }
  if (Op.getOpcode() == ISD::SELECT && Op.getValueType() == MVT::i8)
    return DAG.getNode(MCS51ISD::SELECT8, DL, MVT::i8, Op.getOperand(0),
                       Op.getOperand(1), Op.getOperand(2));
  if (Op.getOpcode() == ISD::SELECT && Op.getValueType() == MVT::i16)
    return DAG.getNode(MCS51ISD::SELECT16, DL, MVT::i16, Op.getOperand(0),
                       Op.getOperand(1), Op.getOperand(2));
  if (Op.getOpcode() == ISD::SRL_PARTS ||
      Op.getOpcode() == ISD::SHL_PARTS ||
      Op.getOpcode() == ISD::SRA_PARTS) {
    unsigned Opcode = Op.getOpcode() == ISD::SHL_PARTS
                          ? MCS51ISD::SHL32_PARTS
                          : Op.getOpcode() == ISD::SRA_PARTS
                                ? MCS51ISD::SRA32_PARTS
                                : MCS51ISD::SRL32_PARTS;
    return DAG.getNode(Opcode, DL, DAG.getVTList(MVT::i16, MVT::i16),
                       Op.getOperand(0), Op.getOperand(1), Op.getOperand(2));
  }
  if (Op.getOpcode() == ISD::SETCC &&
      Op.getOperand(0).getValueType() == MVT::i8) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();
    return LowerByteCompare(Op.getOperand(0), Op.getOperand(1), CC);
  }
  if (Op.getOpcode() == ISD::SETCC &&
      Op.getOperand(0).getValueType() == MVT::i32) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();
    SDValue LoIndex = DAG.getConstant(0, DL, MVT::i16);
    SDValue HiIndex = DAG.getConstant(1, DL, MVT::i16);
    SDValue LHSLo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                Op.getOperand(0), LoIndex);
    SDValue LHSHi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                Op.getOperand(0), HiIndex);
    SDValue RHSLo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                Op.getOperand(1), LoIndex);
    SDValue RHSHi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                Op.getOperand(1), HiIndex);
    if (CC == ISD::SETEQ || CC == ISD::SETNE) {
      SDValue Operands[] = {LHSLo, LHSHi, RHSLo, RHSHi,
                            DAG.getConstant(2, DL, MVT::i8)};
      SDValue Equal = DAG.getNode(MCS51ISD::CMP32, DL, MVT::i8, Operands);
      return CC == ISD::SETEQ
                 ? Equal
                 : DAG.getNode(ISD::XOR, DL, MVT::i8, Equal,
                               DAG.getConstant(1, DL, MVT::i8));
    }
    bool IsSigned = CC == ISD::SETLT || CC == ISD::SETLE ||
                    CC == ISD::SETGT || CC == ISD::SETGE;
    unsigned CompareKind;
    switch (CC) {
    case ISD::SETULT:
    case ISD::SETLT:
      CompareKind = IsSigned ? 1 : 0;
      break;
    case ISD::SETUGE:
    case ISD::SETGE:
      CompareKind = IsSigned ? 4 : 3;
      break;
    case ISD::SETUGT:
    case ISD::SETGT:
      std::swap(LHSLo, RHSLo);
      std::swap(LHSHi, RHSHi);
      CompareKind = IsSigned ? 1 : 0;
      break;
    case ISD::SETULE:
    case ISD::SETLE:
      std::swap(LHSLo, RHSLo);
      std::swap(LHSHi, RHSHi);
      CompareKind = IsSigned ? 4 : 3;
      break;
    default:
      return SDValue();
    }
    SDValue Operands[] = {LHSLo, LHSHi, RHSLo, RHSHi,
                          DAG.getConstant(CompareKind, DL, MVT::i8)};
    return DAG.getNode(MCS51ISD::CMP32, DL, MVT::i8, Operands);
  }
  if (Op.getOpcode() == ISD::SETCC &&
      Op.getOperand(0).getValueType() == MVT::i16) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();
    bool IsSigned = CC == ISD::SETLT || CC == ISD::SETGE ||
                    CC == ISD::SETGT || CC == ISD::SETLE;

    auto GetExtendedByte = [&](SDValue V) -> std::pair<SDValue, int> {
      if ((V.getOpcode() == ISD::ZERO_EXTEND ||
           V.getOpcode() == ISD::SIGN_EXTEND) &&
          V.getOperand(0).getValueType() == MVT::i8)
        return {V.getOperand(0), V.getOpcode() == ISD::SIGN_EXTEND ? 1 : 0};

      // Type legalization folds zext/sext of a byte load into an extending
      // load whose result type is i16. Comparing the truncated bytes avoids
      // forcing those values through the backend's single i16 register (DPTR).
      if (auto *Load = dyn_cast<LoadSDNode>(V)) {
        if (Load->getMemoryVT() == MVT::i8 &&
            (Load->getExtensionType() == ISD::ZEXTLOAD ||
             Load->getExtensionType() == ISD::SEXTLOAD)) {
          int Extension = Load->getExtensionType() == ISD::SEXTLOAD ? 1 : 0;
          return {DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, V), Extension};
        }
      }
      return {SDValue(), -1};
    };

    auto [LHSByte, LHSExtension] = GetExtendedByte(Op.getOperand(0));
    auto [RHSByte, RHSExtension] = GetExtendedByte(Op.getOperand(1));
    if (LHSByte && RHSByte && LHSExtension == RHSExtension &&
        (LHSExtension == 0 || IsSigned || ISD::isIntEqualitySetCC(CC))) {
      if (LHSExtension == 0) {
        switch (CC) {
        case ISD::SETLT:
          CC = ISD::SETULT;
          break;
        case ISD::SETLE:
          CC = ISD::SETULE;
          break;
        case ISD::SETGT:
          CC = ISD::SETUGT;
          break;
        case ISD::SETGE:
          CC = ISD::SETUGE;
          break;
        default:
          break;
        }
      }
      return LowerByteCompare(LHSByte, RHSByte, CC);
    }
    return LowerWordCompare(Op.getOperand(0), Op.getOperand(1), CC, IsSigned);
  }
  if (Op.getOpcode() == ISD::SELECT_CC &&
      Op.getOperand(0).getValueType() == MVT::i8 &&
      Op.getValueType() == MVT::i8) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
    if (CC == ISD::SETEQ || CC == ISD::SETNE) {
      SDValue TrueValue = Op.getOperand(2);
      SDValue FalseValue = Op.getOperand(3);
      if (CC == ISD::SETNE)
        std::swap(TrueValue, FalseValue);
      return DAG.getNode(MCS51ISD::SELECT_EQ8, DL, MVT::i8,
                         Op.getOperand(0), Op.getOperand(1), TrueValue,
                         FalseValue);
    }
    if ((CC == ISD::SETLT || CC == ISD::SETGE) &&
        isa<ConstantSDNode>(Op.getOperand(1)) &&
        cast<ConstantSDNode>(Op.getOperand(1))->isZero()) {
      SDValue TrueValue = Op.getOperand(2);
      SDValue FalseValue = Op.getOperand(3);
      if (CC == ISD::SETGE)
        std::swap(TrueValue, FalseValue);
      return DAG.getNode(MCS51ISD::SELECT_SIGN8, DL, MVT::i8,
                         Op.getOperand(0), TrueValue, FalseValue);
    }
  }
  if (Op.getOpcode() == ISD::SELECT_CC &&
      Op.getOperand(0).getValueType() == MVT::i16 &&
      (Op.getValueType() == MVT::i8 || Op.getValueType() == MVT::i16)) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
    auto *TrueValue = dyn_cast<ConstantSDNode>(Op.getOperand(2));
    auto *FalseValue = dyn_cast<ConstantSDNode>(Op.getOperand(3));
    bool IsSigned = CC == ISD::SETLT || CC == ISD::SETGE ||
                    CC == ISD::SETGT || CC == ISD::SETLE;
    if (Op.getValueType() == MVT::i16) {
      SDValue Result = LowerWordCompare(Op.getOperand(0), Op.getOperand(1),
                                        CC, IsSigned);
      if (Result)
        return DAG.getNode(MCS51ISD::SELECT16, DL, MVT::i16, Result,
                           Op.getOperand(2), Op.getOperand(3));
    }
    if (TrueValue && FalseValue &&
        ((TrueValue->isOne() && FalseValue->isZero()) ||
         (TrueValue->isZero() && FalseValue->isOne()))) {
      SDValue Result = LowerWordCompare(Op.getOperand(0), Op.getOperand(1),
                                        CC, IsSigned);
      if (Result && TrueValue->isZero())
        Result = DAG.getNode(ISD::XOR, DL, MVT::i8, Result,
                             DAG.getConstant(1, DL, MVT::i8));
      return Result;
    }
  }
  if (Op.getOpcode() != ISD::BR_CC)
    return SDValue();

  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  if (LHS.getValueType() == MVT::i16 && RHS.getValueType() == MVT::i16) {
    unsigned Kind;
    switch (CC) {
    case ISD::SETEQ: Kind = 2; break;
    case ISD::SETNE: Kind = 5; break;
    case ISD::SETULT: Kind = 0; break;
    case ISD::SETUGE: Kind = 3; break;
    case ISD::SETLT: Kind = 1; break;
    case ISD::SETGE: Kind = 4; break;
    case ISD::SETUGT:
      std::swap(LHS, RHS);
      Kind = 0;
      break;
    case ISD::SETULE:
      std::swap(LHS, RHS);
      Kind = 3;
      break;
    case ISD::SETGT:
      std::swap(LHS, RHS);
      Kind = 1;
      break;
    case ISD::SETLE:
      std::swap(LHS, RHS);
      Kind = 4;
      break;
    default:
      report_fatal_error("unsupported MCS-51 16-bit branch predicate");
    }
    return DAG.getNode(MCS51ISD::BR_CMP16, DL, MVT::Other, Op.getOperand(0),
                       LHS, RHS, DAG.getConstant(Kind, DL, MVT::i8), Dest);
  }
  if (LHS.getValueType() != MVT::i8 || RHS.getValueType() != MVT::i8)
    report_fatal_error("unsupported MCS-51 conditional branch");

  auto BranchUnsignedConstant = [&](SDValue Value, uint8_t Limit,
                                    bool IsLess) {
    unsigned BranchOpcode = IsLess ? MCS51ISD::BR_ULT : MCS51ISD::BR_UGE;
    return DAG.getNode(BranchOpcode, DL, MVT::Other, Op.getOperand(0), Value,
                       DAG.getConstant(Limit, DL, MVT::i8), Dest);
  };
  if (auto *C = dyn_cast<ConstantSDNode>(RHS)) {
    uint8_t Value = C->getZExtValue();
    switch (CC) {
    case ISD::SETULT:
      return BranchUnsignedConstant(LHS, Value, /*IsLess=*/true);
    case ISD::SETUGE:
      return BranchUnsignedConstant(LHS, Value, /*IsLess=*/false);
    case ISD::SETULE:
      if (Value == 0xff)
        return DAG.getNode(ISD::BR, DL, MVT::Other, Op.getOperand(0), Dest);
      return BranchUnsignedConstant(LHS, Value + 1, /*IsLess=*/true);
    case ISD::SETUGT:
      if (Value == 0xff)
        return Op.getOperand(0);
      return BranchUnsignedConstant(LHS, Value + 1, /*IsLess=*/false);
    default:
      break;
    }
  }
  if (auto *C = dyn_cast<ConstantSDNode>(LHS)) {
    uint8_t Value = C->getZExtValue();
    switch (CC) {
    case ISD::SETULT:
      if (Value == 0xff)
        return Op.getOperand(0);
      return BranchUnsignedConstant(RHS, Value + 1, /*IsLess=*/false);
    case ISD::SETUGE:
      if (Value == 0xff)
        return DAG.getNode(ISD::BR, DL, MVT::Other, Op.getOperand(0), Dest);
      return BranchUnsignedConstant(RHS, Value + 1, /*IsLess=*/true);
    case ISD::SETULE:
      return BranchUnsignedConstant(RHS, Value, /*IsLess=*/false);
    case ISD::SETUGT:
      return BranchUnsignedConstant(RHS, Value, /*IsLess=*/true);
    default:
      break;
    }
  }

  unsigned Opcode;
  switch (CC) {
  case ISD::SETEQ: Opcode = MCS51ISD::BR_EQ; break;
  case ISD::SETNE: Opcode = MCS51ISD::BR_NE; break;
  case ISD::SETULT: Opcode = MCS51ISD::BR_ULT; break;
  case ISD::SETUGE: Opcode = MCS51ISD::BR_UGE; break;
  case ISD::SETUGT:
    Opcode = MCS51ISD::BR_ULT;
    std::swap(LHS, RHS);
    break;
  case ISD::SETULE:
    Opcode = MCS51ISD::BR_UGE;
    std::swap(LHS, RHS);
    break;
  case ISD::SETLT:
    Opcode = MCS51ISD::BR_SLT;
    break;
  case ISD::SETGE:
    Opcode = MCS51ISD::BR_SGE;
    break;
  case ISD::SETGT:
    Opcode = MCS51ISD::BR_SLT;
    std::swap(LHS, RHS);
    break;
  case ISD::SETLE:
    Opcode = MCS51ISD::BR_SGE;
    std::swap(LHS, RHS);
    break;
  default:
    report_fatal_error("unsupported MCS-51 conditional branch predicate");
  }
  return DAG.getNode(Opcode, DL, MVT::Other, Op.getOperand(0), LHS, RHS,
                     Dest);
}

SDValue MCS51TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  InVals.clear();
  SmallVector<CCValAssign, 8> ArgLocs;
  MachineFunction &MF = DAG.getMachineFunction();
  IsVarArg |= MF.getFunction().isVarArg();
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_MCS51);

  for (const CCValAssign &VA : ArgLocs) {
    if (EVT(VA.getLocVT()) != VA.getValVT() ||
        (VA.getValVT() != MVT::i8 && VA.getValVT() != MVT::i16))
      report_fatal_error("unsupported MCS-51 argument type");
    if (VA.isMemLoc()) {
      // Stack arguments are fixed objects below the return address. A word was
      // pushed high byte first, so its low byte sits nearest the return
      // address. Frame lowering resolves the SP-relative address once the
      // callee-saved pushes and the frame size are known.
      MachineFrameInfo &MFI = MF.getFrameInfo();
      auto LoadByte = [&](int64_t SPOffset) {
        int FI = MFI.CreateFixedObject(1, SPOffset, /*IsImmutable=*/true);
        SDValue Ptr = DAG.getFrameIndex(FI, MVT::i16);
        SDValue Load = DAG.getLoad(MVT::i8, DL, Chain, Ptr,
                                   MachinePointerInfo::getFixedStack(MF, FI));
        Chain = Load.getValue(1);
        return Load;
      };
      int64_t First = -(static_cast<int64_t>(VA.getLocMemOffset()) + 2);
      SDValue Low = LoadByte(First);
      if (VA.getValVT() == MVT::i8) {
        InVals.push_back(Low);
        continue;
      }
      SDValue High = LoadByte(First - 1);
      InVals.push_back(DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i16, Low, High));
      continue;
    }
    if (!VA.isRegLoc())
      report_fatal_error("unsupported MCS-51 argument location");
    const TargetRegisterClass *RC = VA.getLocVT() == MVT::i16
                                        ? &MCS51::MCS51GPR16RegClass
                                        : &MCS51::MCS51GPR8RegClass;
    Register LiveIn = MF.addLiveIn(VA.getLocReg(),
                                   RC);
    SDValue Value = DAG.getCopyFromReg(Chain, DL, LiveIn, VA.getLocVT());
    if (VA.getLocInfo() != CCValAssign::Full)
      report_fatal_error("unsupported MCS-51 argument extension");
    InVals.push_back(Value);
  }
  if (IsVarArg) {
    int64_t Offset = -static_cast<int64_t>(CCInfo.getStackSize() + 2);
    int FI = MF.getFrameInfo().CreateFixedObject(1, Offset, true);
    MF.getInfo<MCS51MachineFunctionInfo>()->setVarArgsFrameIndex(FI);
  }
  return Chain;
}

SDValue MCS51TargetLowering::LowerVASTART(SDValue Op,
                                         SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  const auto *FuncInfo = MF.getInfo<MCS51MachineFunctionInfo>();
  if (!FuncInfo->hasVarArgsFrameIndex())
    report_fatal_error("MCS-51 va_start used in a non-variadic function");

  SDLoc DL(Op);
  SDValue Frame = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), MVT::i8);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, Frame, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue MCS51TargetLowering::LowerVAARG(SDValue Op,
                                       SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  SDValue Chain = Op.getOperand(0);
  SDValue VAListAddr = Op.getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  uint64_t Size = DAG.getDataLayout()
                      .getTypeAllocSize(VT.getTypeForEVT(*DAG.getContext()))
                      .getFixedValue();

  // Stack arguments are laid out toward lower IDATA addresses. Keep vararg
  // slots byte aligned so va_arg can walk them by subtracting each value size.
  SDValue VAListLoad = DAG.getLoad(MVT::i8, DL, Chain, VAListAddr,
                                   MachinePointerInfo(SV), Align(1));
  SDValue ArgumentPtr = VAListLoad;
  if (!Size || Size > 8 || (Size & (Size - 1)))
    report_fatal_error("unsupported MCS-51 va_arg size");

  // Caller pushes each scalar from its most significant byte toward its least
  // significant byte. The va_list points at the least significant byte, and
  // subsequent bytes live at successively lower IDATA addresses.
  SmallVector<SDValue, 8> Parts;
  SDValue ArgumentLoadChain = VAListLoad.getValue(1);
  for (uint64_t I = 0; I < Size; ++I) {
    SDValue BytePtr = ArgumentPtr;
    if (I)
      BytePtr = DAG.getNode(ISD::SUB, DL, MVT::i8, ArgumentPtr,
                            DAG.getConstant(I, DL, MVT::i8));
    SDValue Byte = DAG.getLoad(MVT::i8, DL, ArgumentLoadChain, BytePtr,
                               MachinePointerInfo(MCS51::IData), Align(1));
    Parts.push_back(Byte);
    ArgumentLoadChain = Byte.getValue(1);
  }

  EVT IntVT = EVT::getIntegerVT(*DAG.getContext(), Size * 8);
  while (Parts.size() > 1) {
    SmallVector<SDValue, 8> WiderParts;
    for (unsigned I = 0; I < Parts.size(); I += 2) {
      EVT WideVT = EVT::getIntegerVT(*DAG.getContext(),
                                     Parts[I].getValueType().getSizeInBits() * 2);
      WiderParts.push_back(
          DAG.getNode(ISD::BUILD_PAIR, DL, WideVT, Parts[I], Parts[I + 1]));
    }
    Parts = std::move(WiderParts);
  }
  SDValue ArgumentLoad = Parts.front();
  if (VT != IntVT)
    ArgumentLoad = DAG.getNode(ISD::BITCAST, DL, VT, ArgumentLoad);

  SDValue NextPtr = DAG.getNode(
      ISD::SUB, DL, MVT::i8, ArgumentPtr,
      DAG.getConstant(Size, DL, MVT::i8));
  SDValue VAListStore = DAG.getStore(
      ArgumentLoadChain, DL, NextPtr, VAListAddr,
      MachinePointerInfo(SV), Align(1));
  SDValue Results[] = {ArgumentLoad, VAListStore};
  return DAG.getMergeValues(Results, DL);
}

SDValue MCS51TargetLowering::LowerCall(
    CallLoweringInfo &CLI, SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  const SDLoc &DL = CLI.DL;
  MachineFunction &MF = DAG.getMachineFunction();
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  CLI.IsTailCall = false;

  SmallVector<CCValAssign, 8> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(CLI.Outs, CC_MCS51);

  SDValue Chain = CLI.Chain;
  SDValue InGlue;
  unsigned CallFrameSize = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, CallFrameSize, 0, DL);
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SmallVector<std::pair<int64_t, SDValue>, 4> StackArgs;
  for (unsigned I = 0; I < ArgLocs.size(); ++I) {
    const CCValAssign &VA = ArgLocs[I];
    EVT VT = CLI.OutVals[I].getValueType();
    if (EVT(VA.getLocVT()) != VT || (VT != MVT::i8 && VT != MVT::i16))
      report_fatal_error("unsupported MCS-51 call argument");
    if (VA.isRegLoc())
      RegsToPass.emplace_back(VA.getLocReg(), CLI.OutVals[I]);
    else if (VA.isMemLoc())
      StackArgs.emplace_back(VA.getLocMemOffset(), CLI.OutVals[I]);
    else
      report_fatal_error("unsupported MCS-51 call argument location");
  }

  // Preserve stack argument values before register argument copies can
  // overwrite the physical registers that currently hold those values. The
  // hardware stack grows upward, so push arguments in descending ABI offset.
  // Emit zero-valued bytes for alignment holes between arguments.
  std::sort(StackArgs.begin(), StackArgs.end(),
            [](const auto &A, const auto &B) { return A.first < B.first; });
  unsigned StackCursor = CCInfo.getStackSize();
  auto PushPadding = [&]() {
    Chain = DAG.getNode(MCS51ISD::PUSH_PAD8, DL, MVT::Other, Chain);
    --StackCursor;
  };
  for (auto I = StackArgs.rbegin(), E = StackArgs.rend(); I != E; ++I) {
    unsigned ArgSize = I->second.getValueType().getSizeInBits() / 8;
    unsigned ArgEnd = I->first + ArgSize;
    if (ArgEnd > StackCursor)
      report_fatal_error("overlapping MCS-51 stack arguments");
    while (StackCursor > ArgEnd)
      PushPadding();
    SDValue Ops[] = {Chain, I->second};
    unsigned Opcode = I->second.getValueType() == MVT::i16
                          ? MCS51ISD::PUSH_ARG16
                          : MCS51ISD::PUSH_ARG8;
    Chain = DAG.getNode(Opcode, DL, MVT::Other, Ops);
    StackCursor = I->first;
  }
  while (StackCursor)
    PushPadding();

  for (const auto &[Reg, Value] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Value, InGlue);
    InGlue = Chain.getValue(1);
  }
  if (!StackArgs.empty())
    InGlue = SDValue();

  SDValue Callee = CLI.Callee;
  unsigned CallOpcode = MCS51ISD::CALL;
  if (auto *GA = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = GA->getGlobal();
    const auto *F = dyn_cast<Function>(GV);
    unsigned Bank = F && F->hasSection()
                        ? getMCS51CodeBank(F->getSection())
                        : 0;
    const Function &Caller = MF.getFunction();
    unsigned CallerBank = Caller.hasSection()
                              ? getMCS51CodeBank(Caller.getSection())
                              : 0;
    if (Bank && Bank != CallerBank) {
      std::string Thunk = getMCS51BankThunkName(GV->getName());
      MCSymbol *ThunkSym = MF.getContext().getOrCreateSymbol(Thunk);
      Callee = DAG.getTargetExternalSymbol(ThunkSym->getName().data(),
                                           MVT::i16);
    } else {
      Callee = DAG.getTargetGlobalAddress(GV, DL, MVT::i16);
    }
  } else if (auto *ES = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(ES->getSymbol(), MVT::i16);
  else {
    if (Callee.getValueType() != MVT::i16)
      report_fatal_error("unsupported MCS-51 indirect call target type");
    CallOpcode = MCS51ISD::ICALL;
  }

  SmallVector<SDValue, 12> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (const auto &[Reg, Value] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, Value.getValueType()));
  if (InGlue.getNode())
    Ops.push_back(InGlue);
  SDVTList CallVTs = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(CallOpcode, DL, CallVTs, Ops);
  InGlue = Chain.getValue(1);

  // Pop the stack arguments right after the call and glue the pops to it. The
  // type legalizer drops the chain of pure library calls, which would
  // otherwise discard the pops and leak the arguments.
  for (unsigned I = 0; I < CCInfo.getStackSize(); ++I) {
    SDValue Pop = DAG.getNode(MCS51ISD::POP_ARG8, DL,
                              DAG.getVTList(MVT::Other, MVT::Glue), Chain,
                              InGlue);
    Chain = Pop.getValue(0);
    InGlue = Pop.getValue(1);
  }
  Chain = DAG.getCALLSEQ_END(Chain, CallFrameSize, 0, InGlue, DL);
  InGlue = Chain.getValue(1);

  SmallVector<CCValAssign, 2> RetLocs;
  CCState RetInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
  RetInfo.AnalyzeCallResult(CLI.Ins, RetCC_MCS51);
  for (const CCValAssign &VA : RetLocs) {
    SDValue Copy = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(),
                                      InGlue);
    Chain = Copy.getValue(1);
    InGlue = Copy.getValue(2);
    InVals.push_back(Copy.getValue(0));
  }
  return Chain;
}

bool MCS51TargetLowering::CanLowerReturn(
    CallingConv::ID, MachineFunction &, bool,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &,
    const Type *) const {
  // A byte comes back in A, a value of up to four words in IP0-IP3.
  if (Outs.empty())
    return true;
  if (Outs.size() == 1 && Outs.front().VT == MVT::i8)
    return true;
  return Outs.size() <= 4 &&
         llvm::all_of(Outs, [](const ISD::OutputArg &Arg) {
           return Arg.VT == MVT::i16;
         });
}

SDValue MCS51TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID, bool,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  if (Outs.size() != OutVals.size())
    report_fatal_error("MCS-51 return value lowering mismatch");
  if (DAG.getMachineFunction().getFunction().hasFnAttribute("interrupt")) {
    if (!Outs.empty())
      report_fatal_error("MCS-51 interrupt handlers must return void");
    return DAG.getNode(MCS51ISD::RET_INTERRUPT, DL, MVT::Other, Chain);
  }
  if (Outs.size() > 1) {
    // A value of several words: low word first, in IP0 upwards.
    static constexpr MCRegister WordRegs[] = {MCS51::IP0, MCS51::IP1,
                                              MCS51::IP2, MCS51::IP3};
    for (unsigned I = 0; I != Outs.size(); ++I)
      Chain = DAG.getCopyToReg(Chain, DL, WordRegs[I], OutVals[I]);
    return DAG.getNode(MCS51ISD::RET_WORD, DL, MVT::Other, Chain,
                       DAG.getTargetConstant(Outs.size(), DL, MVT::i8));
  } else if (!OutVals.empty() &&
             (Outs.front().VT == MVT::i8 ||
              DAG.getMachineFunction().getFunction().getReturnType()
                  ->isIntegerTy(1) ||
              DAG.getMachineFunction().getFunction().getReturnType()
                  ->isIntegerTy(8))) {
    SDValue RetVal = OutVals.front();
    if (RetVal.getValueType() == MVT::i16)
      RetVal = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, RetVal);
    if (isa<ConstantSDNode>(RetVal)) {
      uint8_t Value = cast<ConstantSDNode>(RetVal)->getZExtValue();
      SDValue Imm = DAG.getConstant(Value, DL, MVT::i8);
      return DAG.getNode(MCS51ISD::RET_A_IMM, DL, MVT::Other, Chain, Imm);
    }
    SDValue Ops[] = {Chain, RetVal};
    return DAG.getNode(MCS51ISD::RET_A, DL, MVT::Other, Ops);
  } else if (!OutVals.empty() && Outs.front().VT == MVT::i16) {
    SDValue RetVal = OutVals.front();
    int Extension = -1;
    SDValue ByteValue;
    if ((RetVal.getOpcode() == ISD::ZERO_EXTEND ||
         RetVal.getOpcode() == ISD::ANY_EXTEND ||
         RetVal.getOpcode() == ISD::SIGN_EXTEND) &&
        RetVal.getOperand(0).getValueType() == MVT::i8) {
      Extension = RetVal.getOpcode() == ISD::SIGN_EXTEND ? 1 : 0;
      ByteValue = RetVal.getOperand(0);
    } else if (RetVal.getValueType() == MVT::i8) {
      Extension = 0;
      ByteValue = RetVal;
    } else if ((RetVal.getOpcode() == ISD::ZERO_EXTEND ||
                RetVal.getOpcode() == ISD::ANY_EXTEND) &&
               RetVal.getOperand(0).getValueType() == MVT::i1) {
      Extension = 0;
      ByteValue = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i8,
                              RetVal.getOperand(0));
    } else if (auto *Load = dyn_cast<LoadSDNode>(RetVal)) {
      if (Load->getMemoryVT() == MVT::i8 &&
          (Load->getExtensionType() == ISD::ZEXTLOAD ||
           Load->getExtensionType() == ISD::SEXTLOAD)) {
        Extension = Load->getExtensionType() == ISD::SEXTLOAD ? 1 : 0;
        ByteValue = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, RetVal);
      }
    }
    if (Extension >= 0) {
      SDValue Ops[] = {Chain, ByteValue};
      unsigned Opcode = Extension ? MCS51ISD::RET_SEXT8
                                  : MCS51ISD::RET_ZEXT8;
      return DAG.getNode(Opcode, DL, MVT::Other, Ops);
    }
    if (RetVal.getValueType() == MVT::i8)
      RetVal = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, RetVal);
    Chain = DAG.getCopyToReg(Chain, DL, MCS51::IP0, RetVal);
  }
  if (!Outs.empty() && Outs.front().VT == MVT::i16)
    return DAG.getNode(MCS51ISD::RET_WORD, DL, MVT::Other, Chain,
                       DAG.getTargetConstant(1, DL, MVT::i8));
  return DAG.getNode(MCS51ISD::RET_GLUE, DL, MVT::Other, Chain);
}

SDValue MCS51TargetLowering::PerformDAGCombine(SDNode *N,
                                                DAGCombinerInfo &DCI) const {
  // Shifting a value assembled from two bytes by eight bits only moves a byte.
  if ((N->getOpcode() == MCS51ISD::SRL16_8 ||
       N->getOpcode() == MCS51ISD::SHL16_8) &&
      N->getOperand(0).getOpcode() == ISD::BUILD_PAIR &&
      N->getOperand(0).getOperand(0).getValueType() == MVT::i8) {
    SelectionDAG &DAG = DCI.DAG;
    SDLoc DL(N);
    SDValue Pair = N->getOperand(0);
    if (N->getOpcode() == MCS51ISD::SRL16_8)
      return DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, Pair.getOperand(1));
    return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i16,
                       DAG.getConstant(0, DL, MVT::i8), Pair.getOperand(0));
  }
  // Store the halves of a 16-bit value assembled from two bytes directly.
  // Going through a 16-bit pointer register only copies the bytes into DPTR
  // and back out again.
  if (N->getOpcode() == ISD::STORE) {
    auto *Store = cast<StoreSDNode>(N);
    SDValue Value = Store->getValue();
    if (!DCI.isBeforeLegalize() && !Store->isIndexed() &&
        !Store->isTruncatingStore() && Store->getMemoryVT() == MVT::i16 &&
        Value.getOpcode() == ISD::BUILD_PAIR &&
        Value.getOperand(0).getValueType() == MVT::i8 &&
        (Store->getAddressSpace() == MCS51::Default ||
         Store->getAddressSpace() == MCS51::XData) &&
        !containsFrameIndex(Store->getBasePtr())) {
      SelectionDAG &DAG = DCI.DAG;
      SDLoc DL(N);
      SDValue Chain = Store->getChain();
      SDValue Ptr = Store->getBasePtr();
      MachineMemOperand::Flags Flags = Store->getMemOperand()->getFlags();
      SDValue Lo = DAG.getStore(Chain, DL, Value.getOperand(0), Ptr,
                                Store->getPointerInfo(), Align(1), Flags);
      SDValue HiPtr = DAG.getMemBasePlusOffset(Ptr, TypeSize::getFixed(1), DL);
      SDValue Hi = DAG.getStore(Chain, DL, Value.getOperand(1), HiPtr,
                                Store->getPointerInfo().getWithOffset(1),
                                Align(1), Flags);
      return DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Lo, Hi);
    }
  }
  if (N->getOpcode() == ISD::SRL && N->getValueType(0) == MVT::i32 &&
      N->hasOneUse() &&
      N->use_begin()->getUser()->getOpcode() == ISD::TRUNCATE &&
      N->use_begin()->getUser()->getValueType(0) == MVT::i8) {
    auto *Amount = dyn_cast<ConstantSDNode>(N->getOperand(1));
    SDValue Input = N->getOperand(0);
    if (Amount && Amount->getZExtValue() <= 24 &&
        Amount->getZExtValue() % 8 == 0 &&
        (Input.getOpcode() == ISD::ADD || Input.getOpcode() == ISD::SUB) &&
        Input.getValueType() == MVT::i32) {
      SDLoc DL(N);
      SDValue Zero = DCI.DAG.getConstant(0, DL, MVT::i16);
      SDValue One = DCI.DAG.getConstant(1, DL, MVT::i16);
      SDValue LHSLo = DCI.DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                      Input.getOperand(0), Zero);
      SDValue LHSHi = DCI.DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                      Input.getOperand(0), One);
      SDValue RHSLo = DCI.DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                      Input.getOperand(1), Zero);
      SDValue RHSHi = DCI.DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                                      Input.getOperand(1), One);
      SDVTList ResultVTs =
          DCI.DAG.getVTList(MVT::i8, MVT::i8, MVT::i8, MVT::i8);
      unsigned Opcode = Input.getOpcode() == ISD::ADD
                            ? MCS51ISD::ADD32_BYTES
                            : MCS51ISD::SUB32_BYTES;
      SDValue Result = DCI.DAG.getNode(Opcode, DL, ResultVTs, LHSLo, LHSHi,
                                       RHSLo, RHSHi);
      return DCI.DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i32,
                             Result.getValue(Amount->getZExtValue() / 8));
    }
  }
  if (N->getOpcode() == ISD::SUB && N->getValueType(0) == MVT::i16) {
    if (auto *C = dyn_cast<ConstantSDNode>(N->getOperand(1))) {
      SDValue Base = N->getOperand(0);
      if (C->getZExtValue() == 1 && !containsFrameIndex(Base) &&
          Base.getOpcode() != ISD::ADDRSPACECAST &&
          Base.getOpcode() != ISD::GlobalAddress)
        return DCI.DAG.getNode(MCS51ISD::SUB16_DEC, SDLoc(N), MVT::i16, Base);
    }
  }
  if (N->getOpcode() == ISD::SHL && N->getValueType(0) == MVT::i16 &&
      N->hasOneUse() &&
      (N->getOperand(0).getOpcode() == ISD::SIGN_EXTEND ||
       N->getOperand(0).getOpcode() == ISD::ZERO_EXTEND) &&
      N->getOperand(0).getOperand(0).getValueType() == MVT::i8) {
    SDNode *User = N->use_begin()->getUser();
    if (User->getOpcode() == ISD::TRUNCATE &&
        User->getValueType(0) == MVT::i8 &&
        User->getOperand(0) == SDValue(N, 0)) {
      SDValue ShiftedByte = DCI.DAG.getNode(
          MCS51ISD::SHL8, SDLoc(N), MVT::i8,
          N->getOperand(0).getOperand(0), N->getOperand(1));
      return DCI.DAG.getNode(ISD::ZERO_EXTEND, SDLoc(N), MVT::i16,
                             ShiftedByte);
    }
  }
  if ((N->getOpcode() == ISD::SHL || N->getOpcode() == ISD::SRL) &&
      N->getValueType(0) == MVT::i8 &&
      !isa<ConstantSDNode>(N->getOperand(1)))
    return DCI.DAG.getNode(N->getOpcode() == ISD::SHL ? MCS51ISD::SHL8
                                                       : MCS51ISD::SRL8,
                           SDLoc(N), MVT::i8, N->getOperand(0),
                           N->getOperand(1));
  if (N->getOpcode() == ISD::TRUNCATE &&
      N->getValueType(0) == MVT::i8) {
    SDValue Shift = N->getOperand(0);
    if (Shift.getOpcode() == ISD::BUILD_PAIR &&
        Shift.getValueType() == MVT::i16 &&
        Shift.getOperand(0).getValueType() == MVT::i8)
      return Shift.getOperand(0);
    if (Shift.getOpcode() == MCS51ISD::SRL16_8 &&
        Shift.getOperand(0).getOpcode() == ISD::BUILD_PAIR &&
        Shift.getOperand(0).getValueType() == MVT::i16 &&
        Shift.getOperand(0).getOperand(1).getValueType() == MVT::i8)
      return Shift.getOperand(0).getOperand(1);
    if (Shift.getOpcode() == ISD::SRA &&
        Shift.getValueType() == MVT::i16 &&
        Shift.getOperand(0).getOpcode() == ISD::SIGN_EXTEND &&
        Shift.getOperand(0).getOperand(0).getValueType() == MVT::i8) {
      SDValue Amount = Shift.getOperand(1);
      bool NarrowAmount =
          (Amount.getOpcode() == ISD::ZERO_EXTEND ||
           Amount.getOpcode() == ISD::SIGN_EXTEND) &&
          Amount.getOperand(0).getValueType() == MVT::i8;
      return DCI.DAG.getNode(NarrowAmount ? MCS51ISD::SRA8_REG
                                          : MCS51ISD::SRA8,
                             SDLoc(N), MVT::i8,
                             Shift.getOperand(0).getOperand(0),
                             NarrowAmount ? Amount.getOperand(0) : Amount);
    }
    if (Shift.getOpcode() == ISD::SRL &&
        Shift.getValueType() == MVT::i16 &&
        Shift.getOperand(0).getOpcode() == ISD::ZERO_EXTEND &&
        Shift.getOperand(0).getOperand(0).getValueType() == MVT::i8) {
      SDValue Amount = Shift.getOperand(1);
      bool NarrowAmount =
          (Amount.getOpcode() == ISD::ZERO_EXTEND ||
           Amount.getOpcode() == ISD::SIGN_EXTEND) &&
          Amount.getOperand(0).getValueType() == MVT::i8;
      return DCI.DAG.getNode(NarrowAmount ? MCS51ISD::SRL8_REG
                                          : MCS51ISD::SRL8,
                             SDLoc(N), MVT::i8,
                             Shift.getOperand(0).getOperand(0),
                             NarrowAmount ? Amount.getOperand(0) : Amount);
    }
    if ((Shift.getOpcode() == ISD::SHL ||
         Shift.getOpcode() == MCS51ISD::SHL16) &&
        Shift.getValueType() == MVT::i16 &&
        (Shift.getOperand(0).getOpcode() == ISD::SIGN_EXTEND ||
         Shift.getOperand(0).getOpcode() == ISD::ZERO_EXTEND) &&
        Shift.getOperand(0).getOperand(0).getValueType() == MVT::i8) {
      SDValue Amount = Shift.getOperand(1);
      bool NarrowAmount =
          (Amount.getOpcode() == ISD::ZERO_EXTEND ||
           Amount.getOpcode() == ISD::SIGN_EXTEND) &&
          Amount.getOperand(0).getValueType() == MVT::i8;
      return DCI.DAG.getNode(NarrowAmount ? MCS51ISD::SHL8_REG
                                          : MCS51ISD::SHL8,
                             SDLoc(N), MVT::i8,
                             Shift.getOperand(0).getOperand(0),
                             NarrowAmount ? Amount.getOperand(0) : Amount);
    }
    return SDValue();
  }
  // A branch on a 32-bit compare works on the four bytes of the operands
  // without materializing the outcome.
  if (N->getOpcode() == ISD::BRCOND && DCI.isBeforeLegalize()) {
    SDValue Cond = N->getOperand(1);
    if (Cond.getOpcode() == ISD::SETCC &&
        Cond.getOperand(0).getValueType() == MVT::i32 && Cond.hasOneUse()) {
      ISD::CondCode CC = cast<CondCodeSDNode>(Cond.getOperand(2))->get();
      SDValue LHS = Cond.getOperand(0), RHS = Cond.getOperand(1);
      unsigned Kind;
      switch (CC) {
      case ISD::SETEQ: Kind = 2; break;
      case ISD::SETNE: Kind = 5; break;
      case ISD::SETULT: Kind = 0; break;
      case ISD::SETUGE: Kind = 3; break;
      case ISD::SETLT: Kind = 1; break;
      case ISD::SETGE: Kind = 4; break;
      case ISD::SETUGT:
        std::swap(LHS, RHS);
        Kind = 0;
        break;
      case ISD::SETULE:
        std::swap(LHS, RHS);
        Kind = 3;
        break;
      case ISD::SETGT:
        std::swap(LHS, RHS);
        Kind = 1;
        break;
      case ISD::SETLE:
        std::swap(LHS, RHS);
        Kind = 4;
        break;
      default:
        return SDValue();
      }
      SelectionDAG &DAG = DCI.DAG;
      SDLoc DL(N);
      SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
      SDValue One = DAG.getConstant(1, DL, MVT::i16);
      SDValue Ops[] = {
          N->getOperand(0),
          DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, LHS, Zero),
          DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, LHS, One),
          DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, RHS, Zero),
          DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, RHS, One),
          DAG.getConstant(Kind, DL, MVT::i8), N->getOperand(2)};
      return DAG.getNode(MCS51ISD::BR_CMP32, DL, MVT::Other, Ops);
    }
  }
  // A 32-bit shift by a constant works on the four bytes of the operand; the
  // generic expansion would shift both halves and merge them.
  if ((N->getOpcode() == ISD::SHL || N->getOpcode() == ISD::SRL ||
       N->getOpcode() == ISD::SRA) &&
      N->getValueType(0) == MVT::i32 && DCI.isBeforeLegalize()) {
    auto *Amount = dyn_cast<ConstantSDNode>(N->getOperand(1));
    if (Amount && Amount->getZExtValue() >= 1 &&
        Amount->getZExtValue() <= 31) {
      SelectionDAG &DAG = DCI.DAG;
      SDLoc DL(N);
      SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
      SDValue One = DAG.getConstant(1, DL, MVT::i16);
      SDValue Lo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                               N->getOperand(0), Zero);
      SDValue Hi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16,
                               N->getOperand(0), One);
      unsigned Opcode = N->getOpcode() == ISD::SHL
                            ? MCS51ISD::SHL32_PARTS
                            : N->getOpcode() == ISD::SRA
                                  ? MCS51ISD::SRA32_PARTS
                                  : MCS51ISD::SRL32_PARTS;
      SDValue Parts = DAG.getNode(
          Opcode, DL, DAG.getVTList(MVT::i16, MVT::i16), Lo, Hi,
          DAG.getConstant(Amount->getZExtValue(), DL, MVT::i16));
      return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Parts.getValue(0),
                         Parts.getValue(1));
    }
  }
  if (N->getOpcode() != ISD::SUB || N->getValueType(0) != MVT::i16)
    return SDValue();

  SDValue Extended = N->getOperand(1);
  SDValue Base = N->getOperand(0);
  if (Base.getValueType() != MVT::i16 ||
      isa<ConstantSDNode>(Base) || containsFrameIndex(Base))
    return SDValue();

  bool IsSigned = false;
  SDValue Byte;
  LoadSDNode *ExtendedLoad = nullptr;
  if (auto *Load = dyn_cast<LoadSDNode>(Extended)) {
    if (Load->getMemoryVT() != MVT::i8 ||
        Load->getExtensionType() == ISD::NON_EXTLOAD ||
        !Extended.hasOneUse())
      return SDValue();
    IsSigned = Load->getExtensionType() == ISD::SEXTLOAD;
    ExtendedLoad = Load;
    Byte = DCI.DAG.getLoad(MVT::i8, SDLoc(N), Load->getChain(),
                           Load->getBasePtr(), Load->getMemOperand());
  } else if ((Extended.getOpcode() == ISD::SIGN_EXTEND ||
              Extended.getOpcode() == ISD::ZERO_EXTEND) &&
             Extended.getOperand(0).getValueType() == MVT::i8) {
    IsSigned = Extended.getOpcode() == ISD::SIGN_EXTEND;
    Byte = Extended.getOperand(0);
  } else {
    return SDValue();
  }

  if (ExtendedLoad)
    DCI.DAG.ReplaceAllUsesOfValueWith(SDValue(ExtendedLoad, 1),
                                      Byte.getValue(1));
  unsigned Opcode = IsSigned ? MCS51ISD::SUB_SEXT8_16
                             : MCS51ISD::SUB_ZEXT8_16;
  return DCI.DAG.getNode(Opcode, SDLoc(N), MVT::i16, Base, Byte);
}

MachineBasicBlock *MCS51TargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *MBB) const {
  struct PhysicalLiveInLowering {
    MachineFunction &MF;
    const TargetInstrInfo &TII;
    DebugLoc DL;

    PhysicalLiveInLowering(MachineFunction &MF, const TargetInstrInfo &TII,
                           const DebugLoc &DL)
        : MF(MF), TII(TII), DL(DL) {}

    ~PhysicalLiveInLowering() {
      MachineRegisterInfo &MRI = MF.getRegInfo();
      for (MachineBasicBlock &Block : MF) {
        if (&Block == &MF.front())
          continue;

        SmallVector<MCPhysReg, 4> LiveIns;
        for (const auto &LI : Block.liveins())
          LiveIns.push_back(LI.PhysReg);
        for (MCPhysReg PhysReg : LiveIns) {
          const TargetRegisterClass *RC = nullptr;
          if (PhysReg == MCS51::A)
            RC = &MCS51::MCS51ARegRegClass;
          else if (PhysReg == MCS51::DPTR)
            RC = &MCS51::MCS51PTRRegClass;
          else if (MCS51::MCS51GPR8RegClass.contains(PhysReg))
            RC = &MCS51::MCS51GPR8RegClass;
          if (!RC || Block.pred_empty())
            continue;

          Register Merged = MRI.createVirtualRegister(RC);
          MachineInstrBuilder Phi =
              BuildMI(Block, Block.begin(), DL, TII.get(TargetOpcode::PHI),
                      Merged);
          for (MachineBasicBlock *Pred : Block.predecessors()) {
            Register EdgeValue = MRI.createVirtualRegister(RC);
            BuildMI(*Pred, Pred->getFirstTerminator(), DL,
                    TII.get(TargetOpcode::COPY), EdgeValue)
                .addReg(PhysReg);
            Phi.addReg(EdgeValue).addMBB(Pred);
          }
          BuildMI(Block, Block.getFirstNonPHI(), DL,
                  TII.get(TargetOpcode::COPY), PhysReg)
              .addReg(Merged);
          Block.removeLiveIn(PhysReg);
        }
      }
      for (MachineBasicBlock &Block : MF) {
        if (&Block == &MF.front() || Block.pred_empty())
          continue;
        bool BIsLiveIn = false;
        for (const auto &LI : Block.liveins())
          BIsLiveIn |= LI.PhysReg == MCS51::B;
        bool BDefined = false;
        bool NeedsBLiveIn = false;
        for (MachineInstr &Instr : Block) {
          for (const MachineOperand &MO : Instr.operands())
            if (MO.isReg() && MO.getReg() == MCS51::B && MO.isUse() &&
                !BDefined)
              NeedsBLiveIn = true;
          for (const MachineOperand &MO : Instr.operands())
            if (MO.isReg() && MO.getReg() == MCS51::B && MO.isDef())
              BDefined = true;
        }
        if (NeedsBLiveIn && !BIsLiveIn)
          Block.addLiveIn(MCS51::B);
      }

      bool Changed;
      do {
        Changed = false;
        for (MachineBasicBlock &Block : MF) {
          bool BIsLiveIn = false;
          for (const auto &LI : Block.liveins())
            BIsLiveIn |= LI.PhysReg == MCS51::B;
          if (!BIsLiveIn)
            continue;
          for (MachineBasicBlock *Pred : Block.predecessors()) {
            MachineBasicBlock::iterator Term = Pred->getFirstTerminator();
            bool HasTermUse = false;
            for (MachineInstr &Instr : *Pred) {
              if (Instr.isTerminator())
                for (const MachineOperand &MO : Instr.operands())
                  HasTermUse |= MO.isReg() && MO.getReg() == MCS51::B &&
                                MO.isUse();
              if (Instr.isTerminator())
                break;
            }
            if (Term != Pred->end() && !HasTermUse) {
              Term->addOperand(MF, MachineOperand::CreateReg(
                                       MCS51::B, false, true));
              Changed = true;
            }

            if (Pred == &MF.front())
              continue;
            bool PredDefinesB = false;
            for (MachineInstr &Instr : *Pred) {
              for (const MachineOperand &MO : Instr.operands())
                if (MO.isReg() && MO.getReg() == MCS51::B && MO.isDef())
                  PredDefinesB = true;
              if (Instr.isTerminator())
                break;
            }
            bool PredHasB = false;
            for (const auto &LI : Pred->liveins())
              PredHasB |= LI.PhysReg == MCS51::B;
            if (!PredDefinesB && !PredHasB) {
              Pred->addLiveIn(MCS51::B);
              Changed = true;
            }
          }
        }
      } while (Changed);
    }
  } LowerPhysicalLiveIns(*MBB->getParent(), *STI.getInstrInfo(), MI.getDebugLoc());

  const TargetInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MII = MI.getIterator();
  const DebugLoc &DL = MI.getDebugLoc();
  MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
  // Loads DPTR with a pointer plus offset in front of the pseudo being
  // expanded, reusing the DPTR value already there when it allows.
  auto SetDptr = [&](Register Base, uint16_t Offset) {
    emitDptrAddress(*MBB, MII, DL, TII, *STI.getRegisterInfo(),
                    *MBB->getParent()->getInfo<MCS51MachineFunctionInfo>(),
                    Base, Offset);
  };
  auto getAccumulatorCopy = [&](Register Reg) -> MachineInstr * {
    if (!Reg.isVirtual())
      return nullptr;
    MachineInstr *Def = MRI.getVRegDef(Reg);
    // Accumulator-state scans below use MII as their endpoint. Keep their
    // iterators in one block; LTO can leave the sole use in a different block
    // from its accumulator-copy definition.
    if (!Def || Def->getParent() != MBB)
      return nullptr;
    if (Def->getOpcode() == TargetOpcode::COPY &&
        Def->getOperand(1).getReg() == MCS51::A)
      return Def;
    if (Def->getOpcode() == MCS51::MOV_RN_A &&
        Def->getOperand(0).getReg() == Reg)
      return Def;
    return nullptr;
  };
  auto valueRemainsInAccumulator = [&](Register Reg,
                                       MachineInstr *&Def) -> bool {
    if (!Reg.isVirtual() || !MRI.hasOneNonDBGUse(Reg))
      return false;
    Def = getAccumulatorCopy(Reg);
    if (!Def)
      return false;
    for (auto I = std::next(Def->getIterator()); I != MII; ++I)
      if (I->modifiesRegister(MCS51::A, STI.getRegisterInfo()))
        return false;
    return true;
  };
  if (MI.getOpcode() == MCS51::RET_A_IMM) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IMM), MCS51::A)
        .addImm(MI.getOperand(0).getImm());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RET_NOA))
        .addReg(MCS51::A, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::RET_ZEXT8 ||
      MI.getOpcode() == MCS51::RET_SEXT8) {
    Register Value = MI.getOperand(0).getReg();
    bool IsSigned = MI.getOpcode() == MCS51::RET_SEXT8;
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Hi;
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo).addReg(Value);
    if (IsSigned) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Value);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
      Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    } else {
      Hi = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_IMM), Hi).addImm(0);
    }
    Register Word = MRI.createVirtualRegister(&MCS51::MCS51GPR16RegClass);
    buildWord(*MBB, MII, DL, TII, Word, Lo, Hi);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::IP0)
        .addReg(Word);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RET_NOA))
        .addReg(MCS51::IP0, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SELECT_SIGN8) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register TrueValue = MI.getOperand(2).getReg();
    Register FalseValue = MI.getOperand(3).getReg();
    MachineBasicBlock *Tail = MBB->splitAt(MI);
    if (Tail == MBB) {
      Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(std::next(MBB->getIterator()), Tail);
      Tail->transferSuccessorsAndUpdatePHIs(MBB);
      MBB->addSuccessor(Tail);
    }
    Tail->removeLiveIn(MCS51::DPTR);
    MachineBasicBlock *NegativeBB =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *NonNegativeBB =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), NegativeBB);
    MF.insert(Tail->getIterator(), NonNegativeBB);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());
    MBB->addSuccessor(NegativeBB);
    MBB->addSuccessor(NonNegativeBB);
    NegativeBB->addSuccessor(Tail);
    NonNegativeBB->addSuccessor(Tail);
    MI.eraseFromParent();

    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JNB)).addImm(0xE7)
        .addMBB(NonNegativeBB);
    Register NegativeCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register NonNegativeCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*NegativeBB, NegativeBB->end(), DL, TII.get(TargetOpcode::COPY),
            NegativeCopy).addReg(TrueValue);
    BuildMI(*NegativeBB, NegativeBB->end(), DL,
            TII.get(MCS51::LJMP)).addMBB(Tail);
    BuildMI(*NonNegativeBB, NonNegativeBB->end(), DL,
            TII.get(TargetOpcode::COPY), NonNegativeCopy).addReg(FalseValue);
    BuildMI(*Tail, Tail->getFirstNonPHI(), DL,
            TII.get(TargetOpcode::PHI), Dst)
        .addReg(NegativeCopy).addMBB(NegativeBB)
        .addReg(NonNegativeCopy).addMBB(NonNegativeBB);
    return Tail;
  }
  if (MI.getOpcode() == MCS51::SELECT_EQ8ri ||
      MI.getOpcode() == MCS51::SELECT_EQ8rr) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    bool IsImmediate = MI.getOpcode() == MCS51::SELECT_EQ8ri;
    Register RHSReg = IsImmediate ? Register() : MI.getOperand(2).getReg();
    int64_t RHSImm = IsImmediate ? MI.getOperand(2).getImm() : 0;
    Register TrueValue = MI.getOperand(3).getReg();
    Register FalseValue = MI.getOperand(4).getReg();
    MachineBasicBlock *Tail = MBB->splitAt(MI);
    if (Tail == MBB) {
      Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(std::next(MBB->getIterator()), Tail);
      Tail->transferSuccessorsAndUpdatePHIs(MBB);
      MBB->addSuccessor(Tail);
    }
    Tail->removeLiveIn(MCS51::DPTR);
    MachineBasicBlock *EqualBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *NotEqualBB =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), EqualBB);
    MF.insert(Tail->getIterator(), NotEqualBB);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());
    MBB->addSuccessor(EqualBB);
    MBB->addSuccessor(NotEqualBB);
    EqualBB->addSuccessor(Tail);
    NotEqualBB->addSuccessor(Tail);
    MI.eraseFromParent();

    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    if (IsImmediate) {
      if (RHSImm)
        BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
            .addImm(RHSImm);
    } else {
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::XRL_A_RN)).addReg(RHSReg);
    }
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JNZ)).addMBB(NotEqualBB);
    Register EqualCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register NotEqualCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*EqualBB, EqualBB->end(), DL, TII.get(TargetOpcode::COPY),
            EqualCopy).addReg(TrueValue);
    BuildMI(*EqualBB, EqualBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Tail);
    BuildMI(*NotEqualBB, NotEqualBB->end(), DL,
            TII.get(TargetOpcode::COPY), NotEqualCopy).addReg(FalseValue);
    BuildMI(*Tail, Tail->getFirstNonPHI(), DL,
            TII.get(TargetOpcode::PHI), Dst)
        .addReg(EqualCopy).addMBB(EqualBB)
        .addReg(NotEqualCopy).addMBB(NotEqualBB);
    return Tail;
  }
  if (MI.getOpcode() == MCS51::SELECT16) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Cond = MI.getOperand(1).getReg();
    Register TrueValue = MI.getOperand(2).getReg();
    Register FalseValue = MI.getOperand(3).getReg();
    Register TrueCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR16RegClass);
    Register FalseCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR16RegClass);
    MachineBasicBlock *Tail = MBB->splitAt(MI);
    if (Tail == MBB) {
      Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(std::next(MBB->getIterator()), Tail);
      Tail->transferSuccessorsAndUpdatePHIs(MBB);
      MBB->addSuccessor(Tail);
    }
    // DPTR is reloaded from the selected virtual value in the tail. The
    // generic split liveness can retain it as a physical live-in even though
    // it is defined before its first use there, which is invalid for an
    // allocatable register in a block containing a PHI.
    Tail->removeLiveIn(MCS51::DPTR);
    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), TrueBB);
    MF.insert(Tail->getIterator(), FalseBB);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());
    MBB->addSuccessor(TrueBB);
    MBB->addSuccessor(FalseBB);
    TrueBB->addSuccessor(Tail);
    FalseBB->addSuccessor(Tail);
    MI.eraseFromParent();

    if (Cond != MCS51::A)
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN)).addReg(Cond);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JZ)).addMBB(FalseBB);
    // MachineBlockPlacement may put the newly-created arms before MBB, so
    // the true arm is not guaranteed to be the fallthrough block.
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(TrueBB);
    BuildMI(*TrueBB, TrueBB->end(), DL, TII.get(TargetOpcode::COPY), TrueCopy)
        .addReg(TrueValue);
    BuildMI(*TrueBB, TrueBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Tail);
    BuildMI(*FalseBB, FalseBB->end(), DL,
            TII.get(TargetOpcode::COPY), FalseCopy)
        .addReg(FalseValue);
    BuildMI(*FalseBB, FalseBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Tail);
    BuildMI(*Tail, Tail->getFirstNonPHI(), DL,
            TII.get(TargetOpcode::PHI), Dst)
        .addReg(TrueCopy).addMBB(TrueBB).addReg(FalseCopy).addMBB(FalseBB);
    return Tail;
  }
  if (MI.getOpcode() == MCS51::SELECT8) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Cond = MI.getOperand(1).getReg();
    Register TrueValue = MI.getOperand(2).getReg();
    Register FalseValue = MI.getOperand(3).getReg();
    Register TrueCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register FalseCopy = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    MachineBasicBlock *Tail = MBB->splitAt(MI);
    if (Tail == MBB) {
      Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(std::next(MBB->getIterator()), Tail);
      Tail->transferSuccessorsAndUpdatePHIs(MBB);
      MBB->addSuccessor(Tail);
    }
    Tail->removeLiveIn(MCS51::DPTR);
    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), TrueBB);
    MF.insert(Tail->getIterator(), FalseBB);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());
    MBB->addSuccessor(TrueBB);
    MBB->addSuccessor(FalseBB);
    TrueBB->addSuccessor(Tail);
    FalseBB->addSuccessor(Tail);
    MI.eraseFromParent();

    if (Cond != MCS51::A)
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN)).addReg(Cond);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JZ)).addMBB(FalseBB);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(TrueBB);
    BuildMI(*TrueBB, TrueBB->end(), DL, TII.get(TargetOpcode::COPY), TrueCopy)
        .addReg(TrueValue);
    BuildMI(*TrueBB, TrueBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Tail);
    BuildMI(*FalseBB, FalseBB->end(), DL,
            TII.get(TargetOpcode::COPY), FalseCopy)
        .addReg(FalseValue);
    BuildMI(*FalseBB, FalseBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Tail);
    BuildMI(*Tail, Tail->getFirstNonPHI(), DL,
            TII.get(TargetOpcode::PHI), Dst)
        .addReg(TrueCopy).addMBB(TrueBB).addReg(FalseCopy).addMBB(FalseBB);
    return Tail;
  }
  unsigned CallOpcode = MI.getOpcode();
  if (CallOpcode == MCS51::ICALL_W) {
    Register Target = MI.getOperand(0).getReg();
    BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
        .addReg(Target);
    BuildMI(*MBB, MI, DL, TII.get(MCS51::ICALL));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADIDATA_GLOBAL8) {
    Register Dst = MI.getOperand(0).getReg();
    MachineMemOperand *MMO = MI.memoperands().front();
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(1), MCS51::IData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(MMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Dst, RegState::Define);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADPDATA_GLOBAL8) {
    Register Dst = MI.getOperand(0).getReg();
    MachineMemOperand *MMO = MI.memoperands().front();
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(1), MCS51::PData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(MMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Dst, RegState::Define);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREIDATA_GLOBAL8) {
    MachineMemOperand *MMO = MI.memoperands().front();
    Register Src = MI.getOperand(1).getReg();
    MachineInstr *Def = nullptr;
    if (valueRemainsInAccumulator(Src, Def))
      Def->eraseFromParent();
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(0), MCS51::IData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREIDATA_GLOBAL8_IMM) {
    MachineMemOperand *MMO = MI.memoperands().front();
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(0), MCS51::IData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_R0_IND_IMM))
        .add(MI.getOperand(1))
        .addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREPDATA_GLOBAL8) {
    MachineMemOperand *MMO = MI.memoperands().front();
    Register Src = MI.getOperand(1).getReg();
    MachineInstr *Def = nullptr;
    if (valueRemainsInAccumulator(Src, Def))
      Def->eraseFromParent();
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(0), MCS51::PData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADPDATA_GLOBAL16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineMemOperand *MMO = MI.memoperands().front();
    MachineFunction &MF = *MBB->getParent();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    MachineMemOperand *LowMMO = MF.getMachineMemOperand(MMO, 0, 1);
    MachineMemOperand *HighMMO = MF.getMachineMemOperand(MMO, 1, 1);
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(1), MCS51::PData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(LowMMO);
    Register Lo = saveAToImag(*MBB, MII, DL, TII, MRI);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_RN))
        .addReg(MCS51::R0, RegState::Define)
        .addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(HighMMO);
    Register Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREPDATA_GLOBAL16) {
    MachineMemOperand *MMO = MI.memoperands().front();
    MachineFunction &MF = *MBB->getParent();
    MachineMemOperand *LowMMO = MF.getMachineMemOperand(MMO, 0, 1);
    MachineMemOperand *HighMMO = MF.getMachineMemOperand(MMO, 1, 1);
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(0), MCS51::PData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(LowMMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_RN))
        .addReg(MCS51::R0, RegState::Define)
        .addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(HighMMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADIDATA_GLOBAL16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineMemOperand *MMO = MI.memoperands().front();
    MachineFunction &MF = *MBB->getParent();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    MachineMemOperand *LowMMO = MF.getMachineMemOperand(MMO, 0, 1);
    MachineMemOperand *HighMMO = MF.getMachineMemOperand(MMO, 1, 1);
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(1), MCS51::IData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(LowMMO);
    Register Lo = saveAToImag(*MBB, MII, DL, TII, MRI);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_RN))
        .addReg(MCS51::R0, RegState::Define)
        .addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(HighMMO);
    Register Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREIDATA_GLOBAL16) {
    MachineMemOperand *MMO = MI.memoperands().front();
    MachineFunction &MF = *MBB->getParent();
    MachineMemOperand *LowMMO = MF.getMachineMemOperand(MMO, 0, 1);
    MachineMemOperand *HighMMO = MF.getMachineMemOperand(MMO, 1, 1);
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    emitIndirectGlobalAddress(*MBB, MII, DL, TII, STI.getRegisterInfo(),
                              MI.getOperand(0), MCS51::IData);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(LowMMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_RN))
        .addReg(MCS51::R0, RegState::Define)
        .addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(HighMMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::MUL16rr) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    Register Bytes[4];
    Register ProductLo = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register ProductHi = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    for (Register &Byte : Bytes)
      Byte = MF.getRegInfo().createVirtualRegister(
          &MCS51::MCS51GPR8RegClass);

    auto CopyDPTR = [&](Register Src) {
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
          .addReg(Src);
    };
    auto ExtractBytes = [&](Register Src, Register Lo, Register Hi) {
      CopyDPTR(Src);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x82);
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo)
          .addReg(MCS51::A);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x83);
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Hi)
          .addReg(MCS51::A);
    };
    ExtractBytes(LHS, Bytes[0], Bytes[1]);
    ExtractBytes(RHS, Bytes[2], Bytes[3]);

    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Bytes[0]);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_RN))
        .addImm(0xF0)
        .addReg(Bytes[2]);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MUL_AB));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), ProductLo)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0xF0);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), ProductHi)
        .addReg(MCS51::A);

    auto AddCrossProductLow = [&](Register L, Register R) {
      Register Partial = MF.getRegInfo().createVirtualRegister(
          &MCS51::MCS51GPR8RegClass);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(L);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_RN))
          .addImm(0xF0)
          .addReg(R);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MUL_AB));
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Partial)
          .addReg(MCS51::A);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(ProductHi);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::ADD_A_RN)).addReg(Partial);
      Register NewProductHi = MF.getRegInfo().createVirtualRegister(
          &MCS51::MCS51GPR8RegClass);
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), NewProductHi)
          .addReg(MCS51::A);
      ProductHi = NewProductHi;
    };
    AddCrossProductLow(Bytes[0], Bytes[3]);
    AddCrossProductLow(Bytes[1], Bytes[2]);

    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(ProductLo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPL_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(ProductHi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPH_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::MUL8TO16rr) {
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_RN))
        .addImm(0xF0)
        .addReg(RHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MUL_AB));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPL_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_B), MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPH_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SRL32_PARTS ||
      MI.getOpcode() == MCS51::SHL32_PARTS ||
      MI.getOpcode() == MCS51::SRA32_PARTS) {
    // A 32-bit shift on the four bytes of two word operands. Whole bytes move
    // by renaming; the remaining 0-7 bits go through a counting loop over all
    // four bytes. The amount is below 32.
    bool IsLeft = MI.getOpcode() == MCS51::SHL32_PARTS;
    bool IsArithmetic = MI.getOpcode() == MCS51::SRA32_PARTS;
    MachineFunction &MF = *MBB->getParent();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    Register DstLo = MI.getOperand(0).getReg();
    Register DstHi = MI.getOperand(1).getReg();
    Register SrcLo = MI.getOperand(2).getReg();
    Register SrcHi = MI.getOperand(3).getReg();
    Register Amount = MI.getOperand(4).getReg();
    auto NewByte = [&]() {
      return MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    };
    auto NewWord = [&]() {
      return MRI.createVirtualRegister(&MCS51::MCS51GPR16RegClass);
    };
    auto SubByte = [&](MachineBasicBlock &Block,
                       MachineBasicBlock::iterator I, Register Word,
                       unsigned Sub) {
      Register Byte = NewByte();
      BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), Byte)
          .addReg(Word, RegState{}, Sub);
      return Byte;
    };
    auto LoopOpcode = IsLeft ? MCS51::SHL32_LOOP
                             : IsArithmetic ? MCS51::SRA32_LOOP
                                            : MCS51::SRL32_LOOP;
    // Emits the loop on four byte registers; returns the four shifted bytes.
    auto EmitLoop = [&](MachineBasicBlock &Block,
                        MachineBasicBlock::iterator I, Register (&B)[4],
                        Register Count, Register (&Out)[4]) {
      Register Lo = NewWord(), Hi = NewWord();
      buildWord(Block, I, DL, TII, Lo, B[0], B[1]);
      buildWord(Block, I, DL, TII, Hi, B[2], B[3]);
      Register OutLo = NewWord(), OutHi = NewWord();
      Register CountOut = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
      BuildMI(Block, I, DL, TII.get(LoopOpcode), OutLo)
          .addDef(OutHi)
          .addDef(CountOut)
          .addReg(Lo)
          .addReg(Hi)
          .addReg(Count);
      Out[0] = SubByte(Block, I, OutLo, MCS51::sub_lo);
      Out[1] = SubByte(Block, I, OutLo, MCS51::sub_hi);
      Out[2] = SubByte(Block, I, OutHi, MCS51::sub_lo);
      Out[3] = SubByte(Block, I, OutHi, MCS51::sub_hi);
    };
    auto SignOf = [&](MachineBasicBlock &Block, MachineBasicBlock::iterator I,
                      Register Byte) {
      BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), MCS51::A)
          .addReg(Byte);
      BuildMI(Block, I, DL, TII.get(MCS51::MOV_C_BIT))
          .addImm(0xE7)
          .addReg(MCS51::A, RegState::Implicit);
      BuildMI(Block, I, DL, TII.get(MCS51::CLR_A));
      BuildMI(Block, I, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
      return saveAToImag(Block, I, DL, TII, MRI);
    };
    auto ZeroByte = [&](MachineBasicBlock &Block,
                        MachineBasicBlock::iterator I) {
      Register Byte = NewByte();
      BuildMI(Block, I, DL, TII.get(MCS51::MOV_IM_IMM), Byte).addImm(0);
      return Byte;
    };
    auto Finish = [&](MachineBasicBlock *Block,
                      MachineBasicBlock::iterator At, Register (&B)[4]) {
      Register Lo = NewWord(), Hi = NewWord();
      buildWord(*Block, At, DL, TII, Lo, B[0], B[1]);
      buildWord(*Block, At, DL, TII, Hi, B[2], B[3]);
      MRI.replaceRegWith(DstLo, Lo);
      MRI.replaceRegWith(DstHi, Hi);
      MI.eraseFromParent();
      return Block;
    };

    MachineInstr *AmountDef = MRI.getVRegDef(Amount);
    bool IsConstant = AmountDef && AmountDef->getOpcode() == MCS51::LDI16 &&
                      AmountDef->getOperand(1).isImm();
    Register B[4] = {SubByte(*MBB, MII, SrcLo, MCS51::sub_lo),
                     SubByte(*MBB, MII, SrcLo, MCS51::sub_hi),
                     SubByte(*MBB, MII, SrcHi, MCS51::sub_lo),
                     SubByte(*MBB, MII, SrcHi, MCS51::sub_hi)};
    if (IsConstant) {
      unsigned K = AmountDef->getOperand(1).getImm() & 0x1f;
      unsigned Bytes = K / 8, Bits = K % 8;
      Register Sign = IsArithmetic && Bytes ? SignOf(*MBB, MII, B[3])
                                            : Register();
      Register R[4];
      for (unsigned I = 0; I != 4; ++I) {
        int From = IsLeft ? int(I) - int(Bytes) : int(I) + int(Bytes);
        if (From >= 0 && From < 4)
          R[I] = B[From];
        else
          R[I] = IsArithmetic ? Sign : ZeroByte(*MBB, MII);
      }
      if (Bits) {
        Register Count = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM), Count).addImm(Bits);
        Register Out[4];
        EmitLoop(*MBB, MII, R, Count, Out);
        for (unsigned I = 0; I != 4; ++I)
          R[I] = Out[I];
      }
      Finish(MBB, MII, R);
      return MBB;
    }

    // Variable amount: a zero count skips the loop.
    MachineBasicBlock *Tail = MBB->splitAt(MI);
    if (Tail == MBB) {
      Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(std::next(MBB->getIterator()), Tail);
      Tail->transferSuccessorsAndUpdatePHIs(MBB);
      MBB->addSuccessor(Tail);
    }
    MachineBasicBlock *Loop = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), Loop);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());
    Register Count = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
    BuildMI(*MBB, MBB->end(), DL, TII.get(TargetOpcode::COPY), Count)
        .addReg(Amount, RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN)).addReg(Count);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JZ)).addMBB(Tail);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Loop);
    MBB->addSuccessor(Tail);
    MBB->addSuccessor(Loop);
    Register Out[4];
    EmitLoop(*Loop, Loop->end(), B, Count, Out);
    Loop->addSuccessor(Tail);
    Register T[4];
    for (unsigned I = 0; I != 4; ++I) {
      T[I] = NewByte();
      BuildMI(*Tail, Tail->begin(), DL, TII.get(TargetOpcode::PHI), T[I])
          .addReg(B[I]).addMBB(MBB)
          .addReg(Out[I]).addMBB(Loop);
    }
    return Finish(Tail, Tail->getFirstNonPHI(), T);
  }
  if (MI.getOpcode() == MCS51::ADD32_BYTESrr ||
      MI.getOpcode() == MCS51::SUB32_BYTESrr ||
      MI.getOpcode() == MCS51::ADD32rr ||
      MI.getOpcode() == MCS51::SUB32rr) {
    MachineFunction &MF = *MBB->getParent();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool IsByteResult = MI.getOpcode() == MCS51::ADD32_BYTESrr ||
                        MI.getOpcode() == MCS51::SUB32_BYTESrr;
    bool IsAdd = MI.getOpcode() == MCS51::ADD32_BYTESrr ||
                 MI.getOpcode() == MCS51::ADD32rr;
    unsigned InputBase = IsByteResult ? 4 : 2;
    Register DstLo = IsByteResult ? Register() : MI.getOperand(0).getReg();
    Register DstHi = IsByteResult ? Register() : MI.getOperand(1).getReg();
    Register DstBytes[4] = {};
    if (IsByteResult)
      for (unsigned I = 0; I != 4; ++I)
        DstBytes[I] = MI.getOperand(I).getReg();
    Register Operands[] = {MI.getOperand(InputBase).getReg(),
                           MI.getOperand(InputBase + 1).getReg(),
                           MI.getOperand(InputBase + 2).getReg(),
                           MI.getOperand(InputBase + 3).getReg()};

    Register InputBytes[4][2] = {};
    bool RHSIsImmediate[2] = {false, false};
    uint16_t RHSImmediate[2] = {0, 0};
    for (unsigned I = 2; I != 4; ++I) {
      MachineInstr *Def = MRI.getVRegDef(Operands[I]);
      if (Def && isWordImmediate(*Def)) {
        RHSIsImmediate[I - 2] = true;
        RHSImmediate[I - 2] =
            static_cast<uint16_t>(Def->getOperand(1).getImm());
      }
    }
    for (unsigned I = 0; I != 4; ++I) {
      if (I >= 2 && RHSIsImmediate[I - 2])
        continue;
      for (unsigned Byte = 0; Byte != 2; ++Byte)
        InputBytes[I][Byte] =
            extractWordByte(*MBB, MII, DL, TII, MRI, Operands[I], Byte != 0);
    }

    Register Sum[4];
    for (unsigned I = 0; I != 4; ++I)
      Sum[I] = IsByteResult ? DstBytes[I]
                            : MF.getRegInfo().createVirtualRegister(
                                  &MCS51::MCS51GPR8RegClass);

    for (unsigned I = 0; I != 4; ++I) {
      unsigned Word = I < 2 ? 0 : 1;
      unsigned Byte = I & 1;
      Register LHS = InputBytes[Word][Byte];
      unsigned RHSWord = I < 2 ? 0 : 1;
      Register RHS = InputBytes[2 + RHSWord][Byte];
      if (RHSIsImmediate[RHSWord]) {
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
        if (!IsAdd && I == 0)
          BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
        uint8_t Imm = (RHSImmediate[RHSWord] >> (Byte * 8)) & 0xff;
        unsigned Opcode = IsAdd
                             ? (I == 0 ? MCS51::ADD_A_IMM
                                       : MCS51::ADDC_A_IMM)
                             : MCS51::SUBB_A_IMM;
        BuildMI(*MBB, MII, DL, TII.get(Opcode), MCS51::A).addImm(Imm);
      } else {
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
        if (!IsAdd && I == 0)
          BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
        unsigned Opcode = IsAdd
                             ? (I == 0 ? MCS51::ADD_A_RN : MCS51::ADDC_A_RN)
                             : MCS51::SUBB_A_RN;
        BuildMI(*MBB, MII, DL, TII.get(Opcode)).addReg(RHS);
      }
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Sum[I])
          .addReg(MCS51::A);
    }

    if (!IsByteResult) {
      buildWord(*MBB, MII, DL, TII, DstLo, Sum[0], Sum[1]);
      buildWord(*MBB, MII, DL, TII, DstHi, Sum[2], Sum[3]);
    }
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::CMP8 || MI.getOpcode() == MCS51::CMP8I) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    bool IsImmediate = MI.getOpcode() == MCS51::CMP8I;
    Register RHSReg = IsImmediate ? Register() : MI.getOperand(2).getReg();
    int64_t RHSImm = IsImmediate ? MI.getOperand(2).getImm() : 0;
    int64_t CompareKind = MI.getOperand(3).getImm();
    if (IsImmediate && CompareKind != 0 && CompareKind != 2 &&
        CompareKind != 3)
      report_fatal_error("unsupported MCS-51 immediate comparison");
    if (CompareKind == 2) {
      MachineBasicBlock *Tail = MBB->splitAt(MI);
      if (Tail == MBB) {
        Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
        MF.insert(std::next(MBB->getIterator()), Tail);
        Tail->transferSuccessorsAndUpdatePHIs(MBB);
        MBB->addSuccessor(Tail);
      }
      Tail->removeLiveIn(MCS51::DPTR);
      MachineBasicBlock *EqualBB =
          MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MachineBasicBlock *NotEqualBB =
          MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(Tail->getIterator(), EqualBB);
      MF.insert(Tail->getIterator(), NotEqualBB);
      while (!MBB->succ_empty())
        MBB->removeSuccessor(MBB->succ_begin());
      MBB->addSuccessor(EqualBB);
      MBB->addSuccessor(NotEqualBB);
      EqualBB->addSuccessor(Tail);
      NotEqualBB->addSuccessor(Tail);
      Register EqualResult = MF.getRegInfo().createVirtualRegister(
          &MCS51::MCS51GPR8RegClass);
      Register NotEqualResult = MF.getRegInfo().createVirtualRegister(
          &MCS51::MCS51GPR8RegClass);
      MI.eraseFromParent();

      // CJNE can perform the comparison and branch in one instruction. It is
      // shorter than moving the operand to A, XORing with the immediate, and
      // testing the result with JNZ.
      if (IsImmediate)
        BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::CJNE_RN))
            .addReg(LHS).addImm(RHSImm).addMBB(NotEqualBB);
      else {
        BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
        BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::XRL_A_RN))
            .addReg(RHSReg);
        BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JNZ)).addMBB(NotEqualBB);
      }
      BuildMI(*EqualBB, EqualBB->end(), DL,
              TII.get(MCS51::MOV_A_IMM), MCS51::A).addImm(1);
      BuildMI(*EqualBB, EqualBB->end(), DL, TII.get(TargetOpcode::COPY),
              EqualResult).addReg(MCS51::A);
      BuildMI(*EqualBB, EqualBB->end(), DL, TII.get(MCS51::LJMP))
          .addMBB(Tail);
      BuildMI(*NotEqualBB, NotEqualBB->end(), DL,
              TII.get(MCS51::MOV_A_IMM), MCS51::A).addImm(0);
      BuildMI(*NotEqualBB, NotEqualBB->end(), DL, TII.get(TargetOpcode::COPY),
              NotEqualResult).addReg(MCS51::A);
      BuildMI(*Tail, Tail->getFirstNonPHI(), DL, TII.get(TargetOpcode::PHI),
              Dst)
          .addReg(EqualResult).addMBB(EqualBB)
          .addReg(NotEqualResult).addMBB(NotEqualBB);
      return Tail;
    }

    if (IsImmediate) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A)
          .addImm(RHSImm);
      if (CompareKind == 3)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::CPL_C));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::RLC_A));
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
          .addReg(MCS51::A);
      MI.eraseFromParent();
      return MBB;
    }

    bool IsSigned = CompareKind == 1 || CompareKind == 4;
    bool IsGreaterEqual = CompareKind == 3 || CompareKind == 4;
    if (IsSigned) {
      // Flipping both sign bits turns signed order into unsigned order.
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(RHSReg);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
          .addImm(0x80);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_A));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
          .addImm(0x80);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    } else {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_RN)).addReg(RHSReg);
    }
    if (IsGreaterEqual)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CPL_C));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RLC_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::ADD16rr || MI.getOpcode() == MCS51::SUB16rr ||
      MI.getOpcode() == MCS51::AND16rr || MI.getOpcode() == MCS51::OR16rr ||
      MI.getOpcode() == MCS51::XOR16rr || MI.getOpcode() == MCS51::ADD16ri ||
      MI.getOpcode() == MCS51::ADDDPTR16ri) {
    // Word arithmetic over the bytes of the operands. A constant operand
    // folds into the byte instructions, and a byte that the operation leaves
    // unchanged is not touched at all.
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    unsigned Opcode = MI.getOpcode();
    bool IsAdd = Opcode == MCS51::ADD16rr || Opcode == MCS51::ADD16ri ||
                 Opcode == MCS51::ADDDPTR16ri;
    bool IsSub = Opcode == MCS51::SUB16rr;
    bool IsLogic = !IsAdd && !IsSub;
    bool HasImm = false;
    uint16_t Imm = 0;
    Register RHS;
    if (Opcode == MCS51::ADD16ri || Opcode == MCS51::ADDDPTR16ri) {
      HasImm = true;
      Imm = MI.getOperand(2).getImm();
    } else {
      RHS = MI.getOperand(2).getReg();
      MachineInstr *Def = MRI.getVRegDef(RHS);
      if (Def && Def->getOpcode() == MCS51::LDI16 &&
          Def->getOperand(1).isImm()) {
        HasImm = true;
        Imm = Def->getOperand(1).getImm();
      }
    }
    if (HasImm && IsAdd && Imm == 0) {
      MRI.replaceRegWith(Dst, LHS);
      MI.eraseFromParent();
      return MBB;
    }
    if (IsAdd) {
      if (MachineInstr *User = singleDptrAddressUser(MRI, Dst, MBB)) {
        // The sum only addresses memory: build it directly in DPTR.
        MachineBasicBlock::iterator At = User->getIterator();
        auto Half = [&](unsigned Half, unsigned Sub, bool Carry, Register Other,
                        unsigned OtherSub) {
          BuildMI(*MBB, At, DL, TII.get(TargetOpcode::COPY), MCS51::A)
              .addReg(LHS, RegState{}, Sub);
          BuildMI(*MBB, At, DL,
                  TII.get(Carry ? MCS51::ADDC_A_IM : MCS51::ADD_A_IM))
              .addReg(Other, RegState{}, OtherSub);
          BuildMI(*MBB, At, DL, TII.get(TargetOpcode::COPY), Half)
              .addReg(MCS51::A);
        };
        if (HasImm) {
          emitDptrAddress(*MBB, At, DL, TII, *STI.getRegisterInfo(),
                          *MBB->getParent()->getInfo<MCS51MachineFunctionInfo>(),
                          LHS, Imm);
        } else {
          Half(MCS51::DPL, MCS51::sub_lo, false, RHS, MCS51::sub_lo);
          Half(MCS51::DPH, MCS51::sub_hi, true, RHS, MCS51::sub_hi);
        }
        markAddressInDptr(*User);
        MI.eraseFromParent();
        return MBB;
      }
    }
    if (HasImm && IsAdd && (Imm == 1 || Imm == 0xFFFF)) {
      BuildMI(*MBB, MII, DL, TII.get(Imm == 1 ? MCS51::INC16 : MCS51::DEC16),
              Dst)
          .addReg(LHS);
      MI.eraseFromParent();
      return MBB;
    }
    auto Operate = [&](bool High, bool First) -> Register {
      unsigned Sub = High ? MCS51::sub_hi : MCS51::sub_lo;
      uint8_t K = High ? Imm >> 8 : Imm & 0xff;
      if (HasImm && IsLogic) {
        // AND with 0xFF, OR or XOR with 0 leave the byte alone.
        if ((Opcode == MCS51::AND16rr && K == 0xFF) ||
            (Opcode != MCS51::AND16rr && K == 0)) {
          Register Same = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
          BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Same)
              .addReg(LHS, RegState{}, Sub);
          return Same;
        }
        if (Opcode == MCS51::AND16rr && K == 0) {
          Register Zero = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
          BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_IMM), Zero).addImm(0);
          return Zero;
        }
      }
      if (HasImm && IsAdd && High && K == 0 && First) {
        // Cannot happen: the high byte of an addition always follows the low.
      }
      if (HasImm && IsAdd && !High && K == 0) {
        // Adding nothing to the low byte produces no carry.
        Register Same = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
        BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Same)
            .addReg(LHS, RegState{}, Sub);
        return Same;
      }
      if (IsSub && First)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
          .addReg(LHS, RegState{}, Sub);
      unsigned ImOpcode, ImmOpcode;
      if (IsAdd) {
        bool Carry = High && !(HasImm && (Imm & 0xff) == 0);
        ImOpcode = Carry ? MCS51::ADDC_A_IM : MCS51::ADD_A_IM;
        ImmOpcode = Carry ? MCS51::ADDC_A_IMM : MCS51::ADD_A_IMM;
      } else if (IsSub) {
        ImOpcode = MCS51::SUBB_A_IM;
        ImmOpcode = MCS51::SUBB_A_IMM;
      } else if (Opcode == MCS51::AND16rr) {
        ImOpcode = MCS51::ANL_A_IM;
        ImmOpcode = MCS51::ANL_A_IMM;
      } else if (Opcode == MCS51::OR16rr) {
        ImOpcode = MCS51::ORL_A_IM;
        ImmOpcode = MCS51::ORL_A_IMM;
      } else {
        ImOpcode = MCS51::XRL_A_IM;
        ImmOpcode = MCS51::XRL_A_IMM;
      }
      if (HasImm)
        BuildMI(*MBB, MII, DL, TII.get(ImmOpcode), MCS51::A).addImm(K);
      else
        BuildMI(*MBB, MII, DL, TII.get(ImOpcode))
            .addReg(RHS, RegState{}, Sub);
      return saveAToImag(*MBB, MII, DL, TII, MRI);
    };
    Register Lo = Operate(false, true);
    Register Hi = Operate(true, false);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::CMP16 || MI.getOpcode() == MCS51::CMP32) {
    // Straight-line comparison over the imaginary bytes of the operands. The
    // kind is 0 unsigned less, 1 signed less, 2 equal, 3 unsigned greater or
    // equal and 4 signed greater or equal.
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    bool Is32 = MI.getOpcode() == MCS51::CMP32;
    Register Dst = MI.getOperand(0).getReg();
    unsigned Words = Is32 ? 4 : 2;
    int64_t CompareKind = MI.getOperand(Words + 1).getImm();
    SmallVector<WordByte, 4> L, R;
    if (Is32) {
      describeWord(MRI, MI.getOperand(1).getReg(), L);
      describeWord(MRI, MI.getOperand(2).getReg(), L);
      describeWord(MRI, MI.getOperand(3).getReg(), R);
      describeWord(MRI, MI.getOperand(4).getReg(), R);
    } else {
      describeWord(MRI, MI.getOperand(1).getReg(), L);
      describeWord(MRI, MI.getOperand(2).getReg(), R);
    }
    emitWordCompare(*MBB, MII, DL, TII, L, R, CompareKind);
    if (CompareKind == 2) {
      // A is zero when the operands match. ADD 0xFF sets CY when it is not;
      // invert it to report equality.
      BuildMI(*MBB, MII, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A).addImm(0xFF);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CPL_C));
    } else if (CompareKind == 3 || CompareKind == 4) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CPL_C));
    }
    // CLR A preserves CY, and RLC moves it into bit zero.
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RLC_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::BRCMP32) {
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    int64_t Kind = MI.getOperand(4).getImm();
    MachineBasicBlock *Target = MI.getOperand(5).getMBB();
    SmallVector<WordByte, 4> L, R;
    describeWord(MRI, MI.getOperand(0).getReg(), L);
    describeWord(MRI, MI.getOperand(1).getReg(), L);
    describeWord(MRI, MI.getOperand(2).getReg(), R);
    describeWord(MRI, MI.getOperand(3).getReg(), R);
    emitWordCompare(*MBB, MII, DL, TII, L, R, Kind);
    unsigned Branch;
    switch (Kind) {
    case 2: Branch = MCS51::JZ; break;
    case 5: Branch = MCS51::JNZ; break;
    case 0:
    case 1: Branch = MCS51::JC; break;
    default: Branch = MCS51::JNC; break;
    }
    BuildMI(*MBB, MII, DL, TII.get(Branch)).addMBB(Target);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::BRCMP16) {
    // Compare and branch without materializing the outcome. A single
    // conditional branch follows the compare: equality leaves zero in A.
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    int64_t Kind = MI.getOperand(2).getImm();
    MachineBasicBlock *Target = MI.getOperand(3).getMBB();
    SmallVector<WordByte, 2> L, R;
    describeWord(MRI, MI.getOperand(0).getReg(), L);
    describeWord(MRI, MI.getOperand(1).getReg(), R);
    emitWordCompare(*MBB, MII, DL, TII, L, R, Kind);
    unsigned Branch;
    switch (Kind) {
    case 2: Branch = MCS51::JZ; break;
    case 5: Branch = MCS51::JNZ; break;
    case 0:
    case 1: Branch = MCS51::JC; break;
    default: Branch = MCS51::JNC; break;
    }
    BuildMI(*MBB, MII, DL, TII.get(Branch)).addMBB(Target);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::RET16) {
    MachineInstrBuilder Ret = BuildMI(*MBB, MII, DL, TII.get(MCS51::RET_NOA));
    static constexpr MCRegister WordRegs[] = {MCS51::IP0, MCS51::IP1,
                                              MCS51::IP2, MCS51::IP3};
    for (int64_t I = 0; I != MI.getOperand(0).getImm(); ++I)
      Ret.addReg(WordRegs[I], RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SRL16_8) {
    // The high byte becomes the low byte.
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Zero = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo)
        .addReg(MI.getOperand(1).getReg(), RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_IMM), Zero).addImm(0);
    buildWord(*MBB, MII, DL, TII, MI.getOperand(0).getReg(), Lo, Zero);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SHL16_8) {
    // The low byte becomes the high byte.
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Hi = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Zero = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Hi)
        .addReg(MI.getOperand(1).getReg(), RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_IMM), Zero).addImm(0);
    buildWord(*MBB, MII, DL, TII, MI.getOperand(0).getReg(), Zero, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SRL16 || MI.getOpcode() == MCS51::SHL16 ||
      MI.getOpcode() == MCS51::SRA16) {
    // 16-bit shifts over the two bytes of the operand. Constant amounts are
    // resolved here; variable amounts also range-check the count. Loops keep
    // both bytes and the count in virtual registers, so nothing depends on
    // DPTR, B or the accumulator surviving a block boundary.
    MachineFunction &MF = *MBB->getParent();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool IsLeft = MI.getOpcode() == MCS51::SHL16;
    bool IsArithmetic = MI.getOpcode() == MCS51::SRA16;
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register Amount = MI.getOperand(2).getReg();
    auto NewByte = [&]() {
      return MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    };
    auto SubByte = [&](MachineBasicBlock &Block,
                       MachineBasicBlock::iterator I, Register Word,
                       unsigned Sub) {
      Register Byte = NewByte();
      BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), Byte)
          .addReg(Word, RegState{}, Sub);
      return Byte;
    };
    // The sign of a byte as 0x00 or 0xFF.
    auto SignOf = [&](MachineBasicBlock &Block, MachineBasicBlock::iterator I,
                      Register Byte) {
      BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), MCS51::A)
          .addReg(Byte);
      BuildMI(Block, I, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
      BuildMI(Block, I, DL, TII.get(MCS51::CLR_A));
      BuildMI(Block, I, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
      return saveAToImag(Block, I, DL, TII, MRI);
    };
    auto Zero = [&](MachineBasicBlock &Block, MachineBasicBlock::iterator I) {
      Register Byte = NewByte();
      BuildMI(Block, I, DL, TII.get(MCS51::MOV_IM_IMM), Byte).addImm(0);
      return Byte;
    };
    // One step of the shift on the bytes in A-based form; Lo and Hi are the
    // current bytes and the new bytes are returned.
    auto Step = [&](MachineBasicBlock &Block, MachineBasicBlock::iterator I,
                    Register Lo, Register Hi, bool TwoBytes) {
      Register NewLo, NewHi;
      if (IsLeft) {
        BuildMI(Block, I, DL, TII.get(MCS51::CLR_C));
        BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), MCS51::A).addReg(Lo);
        BuildMI(Block, I, DL, TII.get(MCS51::RLC_A));
        NewLo = saveAToImag(Block, I, DL, TII, MRI);
        if (TwoBytes) {
          BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), MCS51::A)
              .addReg(Hi);
          BuildMI(Block, I, DL, TII.get(MCS51::RLC_A));
          NewHi = saveAToImag(Block, I, DL, TII, MRI);
        }
      } else {
        // Bytes are shifted from the top: Hi first, its carry feeds Lo.
        Register Top = TwoBytes ? Hi : Lo;
        BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), MCS51::A).addReg(Top);
        if (IsArithmetic)
          BuildMI(Block, I, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
        else
          BuildMI(Block, I, DL, TII.get(MCS51::CLR_C));
        BuildMI(Block, I, DL, TII.get(MCS51::RRC_A));
        Register ShiftedTop = saveAToImag(Block, I, DL, TII, MRI);
        if (TwoBytes) {
          NewHi = ShiftedTop;
          BuildMI(Block, I, DL, TII.get(TargetOpcode::COPY), MCS51::A)
              .addReg(Lo);
          BuildMI(Block, I, DL, TII.get(MCS51::RRC_A));
          NewLo = saveAToImag(Block, I, DL, TII, MRI);
        } else {
          NewLo = ShiftedTop;
        }
      }
      return std::make_pair(NewLo, NewHi);
    };
    // Shifts the bytes by a constant 1..7 in MBB, with a loop when that is
    // shorter than repeating the step. Returns the final bytes; the caller's
    // MBB is replaced by the returned block.
    auto Finish = [&](MachineBasicBlock *Block, Register Lo, Register Hi) {
      buildWord(*Block, Block->getFirstNonPHI(), DL, TII, Dst, Lo, Hi);
      MI.eraseFromParent();
      return Block;
    };
    auto SplitTail = [&]() {
      MachineBasicBlock *Tail = MBB->splitAt(MI);
      if (Tail == MBB) {
        // The pseudo was the last instruction, so there was nothing to split
        // off. Give the loop blocks a real tail to branch to.
        Tail = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
        MF.insert(std::next(MBB->getIterator()), Tail);
        Tail->transferSuccessorsAndUpdatePHIs(MBB);
        MBB->addSuccessor(Tail);
      }
      return Tail;
    };
    auto ClearSuccessors = [](MachineBasicBlock *Block) {
      while (!Block->succ_empty())
        Block->removeSuccessor(Block->succ_begin());
    };
    // Emits the counting loop pseudo on a byte or a word and returns the
    // register that holds the shifted value.
    auto EmitLoop = [&](MachineBasicBlock &Block,
                        MachineBasicBlock::iterator I, Register Value,
                        Register Count, bool TwoBytes) {
      Register Result = TwoBytes ? MRI.createVirtualRegister(
                                       &MCS51::MCS51GPR16RegClass)
                                 : NewByte();
      Register CountOut =
          MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
      unsigned Opcode =
          TwoBytes ? (IsLeft ? MCS51::SHL16_LOOP
                             : IsArithmetic ? MCS51::SRA16_LOOP
                                            : MCS51::SRL16_LOOP)
                   : (IsLeft ? MCS51::SHL8_LOOP
                             : IsArithmetic ? MCS51::SRA8_LOOP
                                            : MCS51::SRL8_LOOP);
      BuildMI(Block, I, DL, TII.get(Opcode), Result)
          .addDef(CountOut)
          .addReg(Value)
          .addReg(Count);
      return Result;
    };

    MachineInstr *AmountDef = MRI.getVRegDef(Amount);
    bool IsConstant = AmountDef && AmountDef->getOpcode() == MCS51::LDI16 &&
                      AmountDef->getOperand(1).isImm();
    if (IsConstant) {
      unsigned K = AmountDef->getOperand(1).getImm() & 0xffff;
      if (K == 0) {
        MRI.replaceRegWith(Dst, Src);
        MI.eraseFromParent();
        return MBB;
      }
      Register Lo = SubByte(*MBB, MII, Src, MCS51::sub_lo);
      Register Hi = SubByte(*MBB, MII, Src, MCS51::sub_hi);
      if (K >= 16) {
        Register Fill = IsArithmetic ? SignOf(*MBB, MII, Hi) : Zero(*MBB, MII);
        Register Other = IsArithmetic ? Fill : Zero(*MBB, MII);
        buildWord(*MBB, MII, DL, TII, Dst, Fill, Other);
        MI.eraseFromParent();
        return MBB;
      }
      unsigned N = K;
      Register OutLo = Lo, OutHi = Hi;
      bool TwoBytes = true;
      if (K >= 8) {
        // Move the byte across first; the rest is a shift of one byte.
        N = K - 8;
        TwoBytes = false;
        if (IsLeft) {
          Register Fill = Zero(*MBB, MII);
          OutHi = Lo; // shifted below
          Lo = Fill;
          OutLo = Fill;
        } else {
          Register Fill = IsArithmetic ? SignOf(*MBB, MII, Hi) : Zero(*MBB, MII);
          OutLo = Hi; // shifted below
          OutHi = Fill;
        }
      }
      // The byte to shift further sits in OutHi (left) or OutLo (right).
      Register &Work = (K >= 8 && IsLeft) ? OutHi : OutLo;
      if (K >= 8 && N == 0) {
        buildWord(*MBB, MII, DL, TII, Dst, OutLo, OutHi);
        MI.eraseFromParent();
        return MBB;
      }
      if (N == 1 || (K >= 8 && N == 1)) {
        if (TwoBytes) {
          auto [NL, NH] = Step(*MBB, MII, Lo, Hi, true);
          OutLo = NL;
          OutHi = NH;
        } else {
          auto [NL, NH] = Step(*MBB, MII, Work, Work, false);
          (void)NH;
          Work = NL;
        }
        buildWord(*MBB, MII, DL, TII, Dst, OutLo, OutHi);
        MI.eraseFromParent();
        return MBB;
      }
      // A loop of N iterations in place.
      Register Count = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM), Count).addImm(N);
      if (TwoBytes) {
        Register Word = MRI.createVirtualRegister(&MCS51::MCS51GPR16RegClass);
        buildWord(*MBB, MII, DL, TII, Word, Lo, Hi);
        Register Result = EmitLoop(*MBB, MII, Word, Count, true);
        MI.getOperand(0).setReg(Dst); // keep Dst as the destination
        MRI.replaceRegWith(Dst, Result);
        MI.eraseFromParent();
        return MBB;
      }
      Register Result = EmitLoop(*MBB, MII, Work, Count, false);
      Work = Result;
      buildWord(*MBB, MII, DL, TII, Dst, OutLo, OutHi);
      MI.eraseFromParent();
      return MBB;
    }

    // Variable amount: values of 16 or more clear the result.
    MachineBasicBlock *Tail = SplitTail();
    MachineBasicBlock *CheckLow = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *LoadCount = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *ZeroBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), CheckLow);
    MF.insert(Tail->getIterator(), LoadCount);
    MF.insert(Tail->getIterator(), ZeroBB);
    Register SrcLo = SubByte(*MBB, MBB->end(), Src, MCS51::sub_lo);
    Register SrcHi = SubByte(*MBB, MBB->end(), Src, MCS51::sub_hi);
    ClearSuccessors(MBB);
    BuildMI(*MBB, MBB->end(), DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Amount, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JNZ)).addMBB(ZeroBB);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(CheckLow);
    MBB->addSuccessor(ZeroBB);
    MBB->addSuccessor(CheckLow);
    BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(TargetOpcode::COPY),
            MCS51::A)
        .addReg(Amount, RegState{}, MCS51::sub_lo);
    BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(MCS51::CLR_C));
    BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(MCS51::SUBB_A_IMM),
            MCS51::A)
        .addImm(16);
    BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(MCS51::JNC)).addMBB(ZeroBB);
    BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(MCS51::LJMP))
        .addMBB(LoadCount);
    CheckLow->addSuccessor(ZeroBB);
    CheckLow->addSuccessor(LoadCount);
    Register Count = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
    BuildMI(*LoadCount, LoadCount->end(), DL, TII.get(TargetOpcode::COPY),
            Count)
        .addReg(Amount, RegState{}, MCS51::sub_lo);
    BuildMI(*LoadCount, LoadCount->end(), DL, TII.get(MCS51::MOV_A_RN))
        .addReg(Count);
    // A zero count leaves the value unchanged.
    BuildMI(*LoadCount, LoadCount->end(), DL, TII.get(MCS51::JZ)).addMBB(Tail);
    LoadCount->addSuccessor(Tail);
    MachineBasicBlock *Loop = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(Tail->getIterator(), Loop);
    LoadCount->addSuccessor(Loop);
    BuildMI(*LoadCount, LoadCount->end(), DL, TII.get(MCS51::LJMP)).addMBB(Loop);
    Register LoopWord = MRI.createVirtualRegister(&MCS51::MCS51GPR16RegClass);
    buildWord(*Loop, Loop->end(), DL, TII, LoopWord, SrcLo, SrcHi);
    Register Shifted = EmitLoop(*Loop, Loop->end(), LoopWord, Count, true);
    Register LoFinal = SubByte(*Loop, Loop->end(), Shifted, MCS51::sub_lo);
    Register HiFinal = SubByte(*Loop, Loop->end(), Shifted, MCS51::sub_hi);
    Loop->addSuccessor(Tail);
    Register ZeroLo = IsArithmetic ? SignOf(*ZeroBB, ZeroBB->end(), SrcHi)
                                   : Zero(*ZeroBB, ZeroBB->end());
    Register ZeroHi = IsArithmetic ? ZeroLo : Zero(*ZeroBB, ZeroBB->end());
    BuildMI(*ZeroBB, ZeroBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(Tail);
    ZeroBB->addSuccessor(Tail);
    Register TailLo = NewByte(), TailHi = NewByte();
    BuildMI(*Tail, Tail->begin(), DL, TII.get(TargetOpcode::PHI), TailHi)
        .addReg(SrcHi).addMBB(LoadCount)
        .addReg(HiFinal).addMBB(Loop)
        .addReg(ZeroHi).addMBB(ZeroBB);
    BuildMI(*Tail, Tail->begin(), DL, TII.get(TargetOpcode::PHI), TailLo)
        .addReg(SrcLo).addMBB(LoadCount)
        .addReg(LoFinal).addMBB(Loop)
        .addReg(ZeroLo).addMBB(ZeroBB);
    return Finish(Tail, TailLo, TailHi);
  }
  if (MI.getOpcode() == MCS51::BUILDPAIR16) {
    buildWord(*MBB, MII, DL, TII, MI.getOperand(0).getReg(),
              MI.getOperand(1).getReg(), MI.getOperand(2).getReg());
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADBIT8) {
    Register Dst = MI.getOperand(0).getReg();
    // Materialize the addressed bit as the canonical byte value 0 or 1.
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT))
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RLC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A), Dst);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREBIT8) {
    // Bit-address space stores consume the low bit of the source byte.
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))
        .addReg(MI.getOperand(1).getReg());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_BIT_C))
        .add(MI.getOperand(0));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADSTACKARG16) {
    Register Dst = MI.getOperand(0).getReg();
    auto Setup = [](MachineInstrBuilder MIB) {
      return MIB.setMIFlag(MachineInstr::FrameSetup);
    };
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A))
        .addImm(0x81);
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A))
        .add(MI.getOperand(1));
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A)))
        .addReg(MCS51::R0, RegState::Define);
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI)))
        .addReg(MCS51::R0);
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A))).addImm(0x82);
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::DEC_RN), MCS51::R0)
              .addReg(MCS51::R0));
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI)))
        .addReg(MCS51::R0);
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A))).addImm(0x83);
    Setup(BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst))
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::PUSHARG8) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))
        .addReg(MI.getOperand(0).getReg());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_DIRECT))
        .addImm(0xE0)
        .addReg(MCS51::A, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::PUSHARG16) {
    // High byte first: the low byte ends up nearest the return address.
    Register Src = MI.getOperand(0).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Hi = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Hi)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_IM)).addReg(Hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_IM)).addReg(Lo);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::PUSHPAD8) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_IMM))
        .addImm(0xE0)
        .addImm(0)
        .addReg(MCS51::A, RegState::ImplicitDefine);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_DIRECT))
        .addImm(0xE0)
        .addReg(MCS51::A, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  auto getNextDirectAddress = [](MachineOperand Addr) {
    if (Addr.isImm())
      Addr.setImm(Addr.getImm() + 1);
    else if (Addr.isGlobal() || Addr.isSymbol())
      Addr.setOffset(Addr.getOffset() + 1);
    else
      report_fatal_error("unsupported MCS-51 direct address operand");
    return Addr;
  };
  if (MI.getOpcode() == MCS51::LOADX16 ||
      MI.getOpcode() == MCS51::LOADCODE16 ||
      MI.getOpcode() == MCS51::LOADCODEABS16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    bool IsCode = MI.getOpcode() == MCS51::LOADCODE16 ||
                  MI.getOpcode() == MCS51::LOADCODEABS16;
    if (MI.getOpcode() == MCS51::LOADCODEABS16)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
          .add(MI.getOperand(1));
    else if (MI.getOperand(1).getReg() != MCS51::DPTR)
      SetDptr(MI.getOperand(1).getReg(), 0);
    Register Bytes[2];
    for (unsigned I = 0; I != 2; ++I) {
      if (I)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
      if (IsCode)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
      BuildMI(*MBB, MII, DL,
              TII.get(IsCode ? MCS51::MOVC_ADPTR : MCS51::MOVX_ADPTR));
      Bytes[I] = saveAToImag(*MBB, MII, DL, TII, MRI);
    }
    buildWord(*MBB, MII, DL, TII, Dst, Bytes[0], Bytes[1]);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADDIRECT8) {
    Register Dst = MI.getOperand(0).getReg();
    MachineInstrBuilder Load =
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
            .add(MI.getOperand(1));
    for (MachineMemOperand *MMO : MI.memoperands())
      Load.addMemOperand(MMO);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADDIRECT16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    MachineOperand AddrLow = MI.getOperand(1);
    MachineOperand AddrHigh = getNextDirectAddress(AddrLow);
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Hi = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_DIRECT), Lo).add(AddrLow);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_DIRECT), Hi).add(AddrHigh);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADXABS8) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADXABS16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    Register Lo = saveAToImag(*MBB, MII, DL, TII, MRI);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    Register Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREDIRECT8) {
    Register Src = MI.getOperand(1).getReg();
    MachineInstr *Copy = nullptr;
    bool SrcAlreadyInA = valueRemainsInAccumulator(Src, Copy);
    if (SrcAlreadyInA)
      Copy->eraseFromParent();
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    MachineInstrBuilder Store =
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A))
            .add(MI.getOperand(0));
    for (MachineMemOperand *MMO : MI.memoperands())
      Store.addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREXABS8) {
    Register Src = MI.getOperand(1).getReg();
    MachineInstr *Def = nullptr;
    bool SrcAlreadyInA = valueRemainsInAccumulator(Src, Def);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(0));
    if (SrcAlreadyInA)
      Def->eraseFromParent();
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    MachineInstrBuilder Store = BuildMI(*MBB, MII, DL,
                                         TII.get(MCS51::MOVX_DPTRA));
    for (MachineMemOperand *MMO : MI.memoperands())
      Store.addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREXABS8_IMM) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IMM), MCS51::A)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREXABS16_IMM) {
    uint16_t Value = static_cast<uint16_t>(MI.getOperand(1).getImm());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IMM), MCS51::A)
        .addImm(Value & 0xff);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IMM), MCS51::A)
        .addImm(Value >> 8);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREXABS16) {
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREDIRECT16) {
    MachineOperand AddrLow = MI.getOperand(0);
    MachineOperand AddrHigh = getNextDirectAddress(AddrLow);
    Register Src = MI.getOperand(1).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Hi = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Hi)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_IM)).add(AddrLow)
        .addReg(Lo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_IM)).add(AddrHigh)
        .addReg(Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADX8) {
    Register Dst = MI.getOperand(0).getReg();
    if (MI.getOperand(1).getReg() != MCS51::DPTR)
      SetDptr(MI.getOperand(1).getReg(), 0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADCODEABS8) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVC_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADCODE8) {
    Register Dst = MI.getOperand(0).getReg();
    if (MI.getOperand(1).getReg() != MCS51::DPTR)
      SetDptr(MI.getOperand(1).getReg(), 0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVC_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREX8) {
    if (MI.getOperand(0).getReg() != MCS51::DPTR)
      SetDptr(MI.getOperand(0).getReg(), 0);
    Register Src = MI.getOperand(1).getReg();
    MachineInstr *Def = nullptr;
    if (valueRemainsInAccumulator(Src, Def))
      Def->eraseFromParent();
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    MachineInstrBuilder Store = BuildMI(*MBB, MII, DL,
                                         TII.get(MCS51::MOVX_DPTRA));
    for (MachineMemOperand *MMO : MI.memoperands())
      Store.addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREX16) {
    Register Addr = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    if (Addr != MCS51::DPTR)
      SetDptr(Addr, 0);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADI16 || MI.getOpcode() == MCS51::LOADP16) {
    Register Dst = MI.getOperand(0).getReg();
    Register Addr = MI.getOperand(1).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register AddrPlus1 = MRI.createVirtualRegister(
        &MCS51::MCS51Indirect8RegClass);
    unsigned LoadOpcode = MI.getOpcode() == MCS51::LOADI16
                              ? MCS51::MOV_A_IND_RI
                              : MCS51::MOVX_A_IND_RI;
    BuildMI(*MBB, MII, DL, TII.get(LoadOpcode)).addReg(Addr);
    Register Lo = saveAToImag(*MBB, MII, DL, TII, MRI);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(AddrPlus1, RegState::Define);
    BuildMI(*MBB, MII, DL, TII.get(LoadOpcode)).addReg(AddrPlus1);
    Register Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREI16 || MI.getOpcode() == MCS51::STOREP16) {
    Register Addr = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register AddrPlus1 = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51Indirect8RegClass);
    unsigned StoreOpcode = MI.getOpcode() == MCS51::STOREI16
                               ? MCS51::MOV_IND_RI_A
                               : MCS51::MOVX_IND_RI_A;
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_lo);
    BuildMI(*MBB, MII, DL, TII.get(StoreOpcode)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(AddrPlus1, RegState::Define);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(Src, RegState{}, MCS51::sub_hi);
    BuildMI(*MBB, MII, DL, TII.get(StoreOpcode)).addReg(AddrPlus1);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADI8 || MI.getOpcode() == MCS51::LOADP8) {
    Register Dst = MI.getOperand(0).getReg();
    Register Addr = MI.getOperand(1).getReg();
    unsigned LoadOpcode = MI.getOpcode() == MCS51::LOADI8
                              ? MCS51::MOV_A_IND_RI
                              : MCS51::MOVX_A_IND_RI;
    BuildMI(*MBB, MII, DL, TII.get(LoadOpcode)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREI8 || MI.getOpcode() == MCS51::STOREP8) {
    Register Addr = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    unsigned StoreOpcode = MI.getOpcode() == MCS51::STOREI8
                               ? MCS51::MOV_IND_RI_A
                               : MCS51::MOVX_IND_RI_A;
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(StoreOpcode)).addReg(Addr);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::BRCOND8) {
    Register Cond = MI.getOperand(0).getReg();
    MachineBasicBlock *Target = MI.getOperand(1).getMBB();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Cond);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::JNZ)).addMBB(Target);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::BR_EQ8ri ||
      MI.getOpcode() == MCS51::BR_EQ8rr ||
      MI.getOpcode() == MCS51::BR_NE8ri ||
      MI.getOpcode() == MCS51::BR_NE8rr ||
      MI.getOpcode() == MCS51::BR_ULT8ri ||
      MI.getOpcode() == MCS51::BR_ULT8rr ||
      MI.getOpcode() == MCS51::BR_UGE8ri ||
      MI.getOpcode() == MCS51::BR_UGE8rr ||
      MI.getOpcode() == MCS51::BR_SLT8ri ||
      MI.getOpcode() == MCS51::BR_SLT8rr ||
      MI.getOpcode() == MCS51::BR_SGE8ri ||
      MI.getOpcode() == MCS51::BR_SGE8rr) {
    Register LHS = MI.getOperand(0).getReg();
    const MachineOperand &RHS = MI.getOperand(1);
    MachineBasicBlock *Target = MI.getOperand(2).getMBB();
    bool IsEqual = MI.getOpcode() == MCS51::BR_EQ8ri ||
                   MI.getOpcode() == MCS51::BR_EQ8rr;
    bool IsNotEqual = MI.getOpcode() == MCS51::BR_NE8ri ||
                      MI.getOpcode() == MCS51::BR_NE8rr;
    bool IsUnsignedLess = MI.getOpcode() == MCS51::BR_ULT8ri ||
                          MI.getOpcode() == MCS51::BR_ULT8rr ||
                          MI.getOpcode() == MCS51::BR_SLT8ri ||
                          MI.getOpcode() == MCS51::BR_SLT8rr;
    bool IsSigned = MI.getOpcode() == MCS51::BR_SLT8ri ||
                    MI.getOpcode() == MCS51::BR_SLT8rr ||
                    MI.getOpcode() == MCS51::BR_SGE8ri ||
                    MI.getOpcode() == MCS51::BR_SGE8rr;
    if (!IsEqual && !IsNotEqual) {
      if (IsSigned) {
        if (RHS.isImm())
          BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IMM), MCS51::A)
              .addImm(RHS.getImm());
        else
          BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(RHS.getReg());
        BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
            .addImm(0x80);
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
            .addReg(MCS51::R1, RegState::Define);
        BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
        BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
            .addImm(0x80);
        BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
        BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_RN))
            .addReg(MCS51::R1);
        BuildMI(*MBB, MII, DL,
                TII.get(IsUnsignedLess ? MCS51::JC : MCS51::JNC))
            .addMBB(Target);
        MI.eraseFromParent();
        return MBB;
      }
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      if (RHS.isImm())
        BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A)
            .addImm(RHS.getImm());
      else
        BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_RN)).addReg(RHS.getReg());
      BuildMI(*MBB, MII, DL,
              TII.get(IsUnsignedLess ? MCS51::JC : MCS51::JNC))
          .addMBB(Target);
      MI.eraseFromParent();
      return MBB;
    }
    unsigned BranchOpcode = IsEqual ? MCS51::JZ : MCS51::JNZ;
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    if (RHS.isImm())
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
          .addImm(RHS.getImm());
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_RN)).addReg(RHS.getReg());
    BuildMI(*MBB, MII, DL, TII.get(BranchOpcode)).addMBB(Target);
    MI.eraseFromParent();
    return MBB;
  }
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  if (MI.getOpcode() == MCS51::ADD_ZEXT8_16 ||
      MI.getOpcode() == MCS51::ADD_SEXT8_16 ||
      MI.getOpcode() == MCS51::SUB_ZEXT8_16 ||
      MI.getOpcode() == MCS51::SUB_SEXT8_16) {
    // Word plus or minus a zero or sign extended byte, straight-line over the
    // bytes of the word. The sign extension byte is formed in B first because
    // forming it clobbers the carry.
    bool IsSubtraction = MI.getOpcode() == MCS51::SUB_ZEXT8_16 ||
                         MI.getOpcode() == MCS51::SUB_SEXT8_16;
    bool IsSigned = MI.getOpcode() == MCS51::ADD_SEXT8_16 ||
                    MI.getOpcode() == MCS51::SUB_SEXT8_16;
    Register Byte = MI.getOperand(2).getReg();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    if (IsSigned) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Byte);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_A));
    }
    Register Lo, Hi;
    if (IsSubtraction) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
          .addReg(LHS, RegState{}, MCS51::sub_lo);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_RN)).addReg(Byte);
    } else {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Byte);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::ADD_A_IM))
          .addReg(LHS, RegState{}, MCS51::sub_lo);
    }
    Lo = saveAToImag(*MBB, MII, DL, TII, MRI);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::A)
        .addReg(LHS, RegState{}, MCS51::sub_hi);
    if (IsSubtraction) {
      if (IsSigned)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_DIRECT), MCS51::A)
            .addImm(0xF0);
      else
        BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A)
            .addImm(0);
    } else {
      if (IsSigned)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::ADDC_A_DIRECT), MCS51::A)
            .addImm(0xF0);
      else
        BuildMI(*MBB, MII, DL, TII.get(MCS51::ADDC_A_IMM), MCS51::A)
            .addImm(0);
    }
    Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::ADD_ZEXT8_IMM ||
      MI.getOpcode() == MCS51::ADD_SEXT8_IMM) {
    bool IsSigned = MI.getOpcode() == MCS51::ADD_SEXT8_IMM;
    uint16_t Immediate = MI.getOperand(2).getImm();
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    if (IsSigned) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_A));
    }
    Register Lo, Hi;
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A)
        .addImm(Immediate & 0xFF);
    Lo = saveAToImag(*MBB, MII, DL, TII, MRI);
    if (IsSigned) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_B), MCS51::A);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::ADDC_A_IMM), MCS51::A)
          .addImm(Immediate >> 8);
    } else {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
      BuildMI(*MBB, MII, DL, TII.get(MCS51::ADDC_A_IMM), MCS51::A)
          .addImm(Immediate >> 8);
    }
    Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::TRUNC16TO8) {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(Src, {}, MCS51::sub_lo);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::ZEXT8TO16) {
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    Register Zero = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IM_IMM), Zero).addImm(0);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Zero);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SEXT8TO16) {
    // The source byte's sign bit supplies every bit of the high byte.
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    Register Lo = MRI.createVirtualRegister(&MCS51::MCS51Imag8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Lo).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
    Register Hi = saveAToImag(*MBB, MII, DL, TII, MRI);
    buildWord(*MBB, MII, DL, TII, Dst, Lo, Hi);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SRA8rr || MI.getOpcode() == MCS51::SRL8rr ||
      MI.getOpcode() == MCS51::SHL8rr ||
      MI.getOpcode() == MCS51::SRA8rrReg ||
      MI.getOpcode() == MCS51::SRL8rrReg ||
      MI.getOpcode() == MCS51::SHL8rrReg) {
    MachineFunction &MF = *MBB->getParent();
    bool IsNarrowCount = MI.getOpcode() == MCS51::SRA8rrReg ||
                         MI.getOpcode() == MCS51::SRL8rrReg ||
                         MI.getOpcode() == MCS51::SHL8rrReg;
    bool IsArithmetic = MI.getOpcode() == MCS51::SRA8rr ||
                        MI.getOpcode() == MCS51::SRA8rrReg;
    bool IsLeft = MI.getOpcode() == MCS51::SHL8rr ||
                  MI.getOpcode() == MCS51::SHL8rrReg;
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register Amount = MI.getOperand(2).getReg();
    MachineBasicBlock *Tail = MBB->splitAt(MI);
    if (Tail == MBB) {
      // The pseudo was the last instruction, so there was nothing to split
      // off. Give the loop blocks a real tail to branch to.
      MachineFunction &TailMF = *MBB->getParent();
      Tail = TailMF.CreateMachineBasicBlock(MBB->getBasicBlock());
      TailMF.insert(std::next(MBB->getIterator()), Tail);
      Tail->transferSuccessorsAndUpdatePHIs(MBB);
      MBB->addSuccessor(Tail);
    }
    MachineBasicBlock *CheckLow = IsNarrowCount
                                      ? nullptr
                                      : MF.CreateMachineBasicBlock(
                                            MBB->getBasicBlock());
    MachineBasicBlock *CheckCount =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *Saturate =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *StartShift =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *ShiftLoop =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *ShiftDone =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MachineBasicBlock *ZeroShift =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    if (CheckLow)
      MF.insert(Tail->getIterator(), CheckLow);
    for (MachineBasicBlock *Block : {CheckCount, StartShift, ShiftLoop,
                                     ShiftDone, ZeroShift, Saturate})
      MF.insert(Tail->getIterator(), Block);

    if (IsNarrowCount) {
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_A_RN))
          .addReg(Amount);
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::MOV_DIRECT_A))
          .addImm(0xF0);
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::SUBB_A_IMM), MCS51::A)
          .addImm(8);
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JNC)).addMBB(Saturate);
    } else {
      BuildMI(*MBB, MBB->end(), DL, TII.get(TargetOpcode::COPY), MCS51::A)
          .addReg(Amount, RegState{}, MCS51::sub_hi);
      BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::JNZ)).addMBB(Saturate);
      BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(TargetOpcode::COPY),
              MCS51::A)
          .addReg(Amount, RegState{}, MCS51::sub_lo);
      BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(MCS51::CLR_C));
      BuildMI(*CheckLow, CheckLow->end(), DL,
              TII.get(MCS51::SUBB_A_IMM), MCS51::A)
          .addImm(8);
      BuildMI(*CheckLow, CheckLow->end(), DL, TII.get(MCS51::JNC))
          .addMBB(Saturate);
    }

    if (IsNarrowCount) {
      BuildMI(*CheckCount, CheckCount->end(), DL,
              TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    } else {
      BuildMI(*CheckCount, CheckCount->end(), DL, TII.get(TargetOpcode::COPY),
              MCS51::A)
          .addReg(Amount, RegState{}, MCS51::sub_lo);
      BuildMI(*CheckCount, CheckCount->end(), DL,
              TII.get(MCS51::MOV_DIRECT_A))
          .addImm(0xF0);
      BuildMI(*CheckCount, CheckCount->end(), DL,
              TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    }
    BuildMI(*CheckCount, CheckCount->end(), DL, TII.get(MCS51::JZ))
        .addMBB(ZeroShift);

    BuildMI(*StartShift, StartShift->end(), DL, TII.get(MCS51::MOV_A_RN))
        .addReg(Src);
    if (IsLeft)
      BuildMI(*ShiftLoop, ShiftLoop->end(), DL, TII.get(MCS51::CLR_C));
    else if (IsArithmetic)
      BuildMI(*ShiftLoop, ShiftLoop->end(), DL, TII.get(MCS51::MOV_C_BIT))
          .addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
    else
      BuildMI(*ShiftLoop, ShiftLoop->end(), DL, TII.get(MCS51::CLR_C));
    BuildMI(*ShiftLoop, ShiftLoop->end(), DL,
            TII.get(IsLeft ? MCS51::RLC_A : MCS51::RRC_A));
    BuildMI(*ShiftLoop, ShiftLoop->end(), DL, TII.get(MCS51::DJNZ_DIRECT))
        .addImm(0xF0)
        .addMBB(ShiftLoop)
        .addReg(MCS51::A, RegState::Implicit);
    BuildMI(*ShiftDone, ShiftDone->end(), DL, TII.get(MCS51::LJMP))
        .addMBB(Tail)
        .addReg(MCS51::A, RegState::Implicit);

    BuildMI(*ZeroShift, ZeroShift->end(), DL, TII.get(MCS51::MOV_A_RN))
        .addReg(Src);
    BuildMI(*ZeroShift, ZeroShift->end(), DL, TII.get(MCS51::LJMP))
        .addMBB(Tail)
        .addReg(MCS51::A, RegState::Implicit);

    if (IsArithmetic) {
      BuildMI(*Saturate, Saturate->end(), DL, TII.get(MCS51::MOV_A_RN))
          .addReg(Src);
      BuildMI(*Saturate, Saturate->end(), DL, TII.get(MCS51::MOV_C_BIT))
          .addImm(0xE7)
          .addReg(MCS51::A, RegState::Implicit);
      BuildMI(*Saturate, Saturate->end(), DL, TII.get(MCS51::CLR_A));
      BuildMI(*Saturate, Saturate->end(), DL, TII.get(MCS51::SUBB_A_IMM),
              MCS51::A)
          .addImm(0);
    } else {
      BuildMI(*Saturate, Saturate->end(), DL, TII.get(MCS51::CLR_A));
    }
    BuildMI(*Saturate, Saturate->end(), DL, TII.get(MCS51::LJMP))
        .addMBB(Tail)
        .addReg(MCS51::A, RegState::Implicit);

    auto ClearSuccessors = [](MachineBasicBlock *Block) {
      while (!Block->succ_empty())
        Block->removeSuccessor(Block->succ_begin());
    };
    ClearSuccessors(MBB);
    MBB->addSuccessor(IsNarrowCount ? CheckCount : CheckLow);
    MBB->addSuccessor(Saturate);
    if (CheckLow) {
      CheckLow->addSuccessor(CheckCount);
      CheckLow->addSuccessor(Saturate);
    }
    CheckCount->addSuccessor(ZeroShift);
    CheckCount->addSuccessor(StartShift);
    if (IsNarrowCount)
      CheckCount->addLiveIn(MCS51::B);
    StartShift->addSuccessor(ShiftLoop);
    ShiftLoop->addSuccessor(ShiftLoop);
    ShiftLoop->addSuccessor(ShiftDone);
    ShiftLoop->addLiveIn(MCS51::A);
    ShiftLoop->addLiveIn(MCS51::B);
    StartShift->addLiveIn(MCS51::B);
    ShiftDone->addSuccessor(Tail);
    ShiftDone->addLiveIn(MCS51::A);
    ZeroShift->addSuccessor(Tail);
    Saturate->addSuccessor(Tail);
    Tail->addLiveIn(MCS51::A);

    BuildMI(*Tail, Tail->getFirstNonPHI(), DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Dst, RegState::Define);
    MI.eraseFromParent();
    return Tail;
  }
  if (MI.getOpcode() == MCS51::SHL8ri || MI.getOpcode() == MCS51::SRL8ri ||
      MI.getOpcode() == MCS51::SRA8ri) {
    unsigned Amount = MI.getOperand(2).getImm();
    bool IsArithmetic = MI.getOpcode() == MCS51::SRA8ri;
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    if (!IsArithmetic && Amount >= 8) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    } else {
      unsigned RotateOpcode = MI.getOpcode() == MCS51::SHL8ri
                                  ? MCS51::RLC_A
                                  : MCS51::RRC_A;
      Amount = std::min(Amount, 8u);
      while (Amount--) {
        if (IsArithmetic)
          BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_C_BIT)).addImm(0xE7)
        .addReg(MCS51::A, RegState::Implicit);
        else
          BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
        BuildMI(*MBB, MII, DL, TII.get(RotateOpcode));
      }
    }
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::MUL8ri) {
    Register LHS = MI.getOperand(1).getReg();
    bool LHSAlreadyInA = false;
    if (LHS.isVirtual() && MRI.hasOneNonDBGUse(LHS)) {
      MachineInstr *Copy = getAccumulatorCopy(LHS);
      if (Copy) {
        LHSAlreadyInA = true;
        for (auto I = std::next(Copy->getIterator()); LHSAlreadyInA &&
                                                    I != MII;
             ++I)
          LHSAlreadyInA =
              !I->modifiesRegister(MCS51::A, STI.getRegisterInfo());
      }
    }
    if (LHSAlreadyInA)
      MRI.getVRegDef(LHS)->eraseFromParent();
    else
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_IMM))
        .addImm(0xF0)
        .addImm(MI.getOperand(2).getImm());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MUL_AB));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY),
            MI.getOperand(0).getReg())
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::MUL8rr) {
    Register RHS = MI.getOperand(2).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_RN))
        .addImm(0xF0)
        .addReg(RHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MUL_AB));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::UDIV8rr || MI.getOpcode() == MCS51::UREM8rr) {
    Register RHS = MI.getOperand(2).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_RN))
        .addImm(0xF0)
        .addReg(RHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::DIV_AB));
    if (MI.getOpcode() == MCS51::UREM8rr)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  unsigned AccOpcode;
  bool IsImmediate = MI.getOpcode() == MCS51::ADD8ri ||
                     MI.getOpcode() == MCS51::SUB8ri ||
                     MI.getOpcode() == MCS51::AND8ri ||
                     MI.getOpcode() == MCS51::OR8ri ||
                     MI.getOpcode() == MCS51::XOR8ri;
  bool IsSubtraction = MI.getOpcode() == MCS51::SUB8rr ||
                       MI.getOpcode() == MCS51::SUB8ri;

  int64_t Immediate = IsImmediate ? MI.getOperand(2).getImm() : 0;
  bool IsComplement = MI.getOpcode() == MCS51::XOR8ri &&
                      static_cast<uint8_t>(Immediate) == 0xff;
  bool IsIncrement = MI.getOpcode() == MCS51::ADD8ri && Immediate == 1;
  bool IsDecrement = (MI.getOpcode() == MCS51::ADD8ri && Immediate == -1) ||
                     (MI.getOpcode() == MCS51::SUB8ri && Immediate == 1);
  if (IsIncrement || IsDecrement) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL,
            TII.get(IsIncrement ? MCS51::INC_A : MCS51::DEC_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }

  MachineInstr *LHSCopy = nullptr;
  bool LHSAlreadyInA = false;
  if (LHS.isVirtual()) {
    LHSCopy = getAccumulatorCopy(LHS);
    if (LHSCopy) {
      LHSAlreadyInA = true;
      for (auto I = std::next(LHSCopy->getIterator()); LHSAlreadyInA &&
                                                  I != MII;
           ++I)
        LHSAlreadyInA =
            !I->modifiesRegister(MCS51::A, STI.getRegisterInfo());
    }
  }

  if (!IsImmediate) {
    unsigned DirectOpcode = 0;
    unsigned IndirectOpcode = 0;
    switch (MI.getOpcode()) {
    case MCS51::ADD8rr:
      DirectOpcode = MCS51::ADD_A_DIRECT;
      IndirectOpcode = MCS51::ADD_A_IND_RI;
      break;
    case MCS51::SUB8rr:
      DirectOpcode = MCS51::SUBB_A_DIRECT;
      IndirectOpcode = MCS51::SUBB_A_IND_RI;
      break;
    case MCS51::AND8rr:
      DirectOpcode = MCS51::ANL_A_DIRECT;
      IndirectOpcode = MCS51::ANL_A_IND_RI;
      break;
    case MCS51::OR8rr:
      DirectOpcode = MCS51::ORL_A_DIRECT;
      IndirectOpcode = MCS51::ORL_A_IND_RI;
      break;
    case MCS51::XOR8rr:
      DirectOpcode = MCS51::XRL_A_DIRECT;
      IndirectOpcode = MCS51::XRL_A_IND_RI;
      break;
    default:
      break;
    }

    Register RHS = MI.getOperand(2).getReg();
    if (DirectOpcode && RHS != LHS && RHS.isVirtual() &&
        MRI.hasOneNonDBGUse(RHS)) {
      MachineInstr *Copy = getAccumulatorCopy(RHS);
      if (Copy) {
        auto LoadIt = Copy->getIterator();
        while (LoadIt != MBB->begin() && std::prev(LoadIt)->isDebugInstr())
          --LoadIt;
        if (LoadIt != MBB->begin()) {
          MachineInstr *Load = &*std::prev(LoadIt);
          bool IsDirectLoad = Load->getOpcode() == MCS51::MOV_A_DIRECT;
          bool IsIndirectLoad = Load->getOpcode() == MCS51::MOV_A_IND_RI;
          bool CanFold =
              !Load->memoperands_empty() &&
              ((IsDirectLoad && DirectOpcode) ||
               (IsIndirectLoad && IndirectOpcode));
          Register IndirectAddress = IsIndirectLoad
                                         ? Load->getOperand(0).getReg()
                                         : Register();
          for (auto I = std::next(Copy->getIterator()); CanFold && I != MII;
               ++I)
            CanFold = !I->mayLoadOrStore() &&
                      !I->hasUnmodeledSideEffects() && !I->isCall() &&
                      !I->isTerminator() &&
                      !I->readsRegister(MCS51::A, STI.getRegisterInfo()) &&
                      (!IsIndirectLoad ||
                       !I->modifiesRegister(IndirectAddress,
                                            STI.getRegisterInfo()));
          if (CanFold) {
            if (LHSCopy && LHSCopy != Copy) {
              LHSAlreadyInA = true;
              for (auto I = std::next(LHSCopy->getIterator());
                   LHSAlreadyInA && I != MII; ++I)
                if (&*I != Load && &*I != Copy)
                  LHSAlreadyInA =
                      !I->modifiesRegister(MCS51::A, STI.getRegisterInfo());
            }
            if (LHSAlreadyInA && LHSCopy &&
                MRI.hasOneNonDBGUse(LHS))
              LHSCopy->eraseFromParent();
            if (!LHSAlreadyInA)
              BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
            if (IsSubtraction)
              BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
            MachineInstrBuilder FoldedOp;
            if (IsDirectLoad) {
              FoldedOp =
                  BuildMI(*MBB, MII, DL, TII.get(DirectOpcode), MCS51::A)
                      .add(Load->getOperand(1));
            } else {
              FoldedOp =
                  BuildMI(*MBB, MII, DL, TII.get(IndirectOpcode), MCS51::A)
                      .addReg(IndirectAddress);
            }
            for (MachineMemOperand *MMO : Load->memoperands())
              FoldedOp.addMemOperand(MMO);
            BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
                .addReg(MCS51::A);
            Load->eraseFromParent();
            Copy->eraseFromParent();
            MI.eraseFromParent();
            return MBB;
          }
        }
      }
    }
  }

  bool RHSIsLHS = !IsImmediate && MI.getOperand(2).getReg() == LHS;
  switch (MI.getOpcode()) {
  case MCS51::ADD8rr:
    AccOpcode = MCS51::ADD_A_RN;
    break;
  case MCS51::SUB8rr:
    AccOpcode = MCS51::SUBB_A_RN;
    break;
  case MCS51::AND8rr:
    AccOpcode = MCS51::ANL_A_RN;
    break;
  case MCS51::OR8rr:
    AccOpcode = MCS51::ORL_A_RN;
    break;
  case MCS51::XOR8rr:
    AccOpcode = MCS51::XRL_A_RN;
    break;
  case MCS51::ADD8ri:
    AccOpcode = MCS51::ADD_A_IMM;
    break;
  case MCS51::SUB8ri:
    AccOpcode = MCS51::SUBB_A_IMM;
    break;
  case MCS51::AND8ri:
    AccOpcode = MCS51::ANL_A_IMM;
    break;
  case MCS51::OR8ri:
    AccOpcode = MCS51::ORL_A_IMM;
    break;
  case MCS51::XOR8ri:
    AccOpcode = IsComplement ? MCS51::CPL_A : MCS51::XRL_A_IMM;
    break;
  default:
    llvm_unreachable("unexpected MCS-51 ALU pseudo");
  }
  if (!IsImmediate && !LHSAlreadyInA && !RHSIsLHS &&
      MRI.hasOneNonDBGUse(LHS)) {
    Register RHS = MI.getOperand(2).getReg();
    MachineInstr *RHSDef = nullptr;
    if (valueRemainsInAccumulator(RHS, RHSDef)) {
      RHSDef->eraseFromParent();
      if (IsSubtraction) {
        Register SwappedLHS = MRI.createVirtualRegister(&MCS51::MCS51GPR8RegClass);
        BuildMI(*MBB, MII, DL, TII.get(MCS51::XCH_A_RN), SwappedLHS)
            .addReg(LHS);
        LHS = SwappedLHS;
      }
      if (IsSubtraction)
        BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
      BuildMI(*MBB, MII, DL, TII.get(AccOpcode)).addReg(LHS);
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
          .addReg(MCS51::A);
      MI.eraseFromParent();
      return MBB;
    }
  }
  if (LHSAlreadyInA && !RHSIsLHS && LHSCopy &&
      MRI.hasOneNonDBGUse(LHS))
    LHSCopy->eraseFromParent();
  if (!LHSAlreadyInA)
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
  if (IsSubtraction)
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
  if (IsImmediate && !IsComplement) {
    BuildMI(*MBB, MII, DL, TII.get(AccOpcode), MCS51::A)
        .addImm(MI.getOperand(2).getImm());
  } else if (IsComplement) {
    BuildMI(*MBB, MII, DL, TII.get(AccOpcode));
  } else {
    BuildMI(*MBB, MII, DL, TII.get(AccOpcode))
        .addReg(MI.getOperand(2).getReg());
  }
  BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
      .addReg(MCS51::A);
  MI.eraseFromParent();
  return MBB;
}
