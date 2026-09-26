#include "MCS51ISelLowering.h"
#include "MCS51Banking.h"
#include "MCS51SelectionDAGInfo.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "MCS51GenCallingConv.inc"

MCS51TargetLowering::MCS51TargetLowering(const TargetMachine &TM,
                                         const MCS51Subtarget &STI)
    : TargetLowering(TM, STI), STI(STI) {
  addRegisterClass(MVT::i8, &MCS51::MCS51GPR8RegClass);
  addRegisterClass(MVT::i16, &MCS51::MCS51PTRRegClass);
  setOperationAction(ISD::UDIV, MVT::i8, Legal);
  setOperationAction(ISD::UREM, MVT::i8, Legal);
  setOperationAction(ISD::SHL, MVT::i8, Legal);
  setOperationAction(ISD::SRL, MVT::i8, Legal);
  setOperationAction(ISD::SRL, MVT::i16, Custom);
  setOperationAction(ISD::SETCC, MVT::i16, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i8, Custom);
  setOperationAction(ISD::BR_CC, MVT::i8, Custom);
  setOperationAction(ISD::BR_CC, MVT::i16, Custom);
  setBooleanContents(ZeroOrOneBooleanContent);
  setStackPointerRegisterToSaveRestore(MCS51::SP);
  computeRegisterProperties(STI.getRegisterInfo());
}

EVT MCS51TargetLowering::getSetCCResultType(const DataLayout &, LLVMContext &,
                                            EVT VT) const {
  assert(!VT.isVector() && "MCS-51 does not support vector comparisons");
  return MVT::i8;
}

MVT MCS51TargetLowering::getRegisterTypeForCallingConv(
    LLVMContext &Context, CallingConv::ID CC, EVT VT) const {
  if (VT == MVT::i32)
    return MVT::i8;
  return TargetLowering::getRegisterTypeForCallingConv(Context, CC, VT);
}

unsigned MCS51TargetLowering::getNumRegistersForCallingConv(
    LLVMContext &Context, CallingConv::ID CC, EVT VT) const {
  if (VT == MVT::i32)
    return 4;
  return TargetLowering::getNumRegistersForCallingConv(Context, CC, VT);
}

SDValue MCS51TargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto LowerWordCompare = [&](SDValue LHS, SDValue RHS, ISD::CondCode CC,
                              bool IsSigned) -> SDValue {
    bool Invert = false;
    switch (CC) {
    case ISD::SETULT:
    case ISD::SETLT:
      break;
    case ISD::SETUGE:
    case ISD::SETGE:
      Invert = true;
      break;
    case ISD::SETUGT:
    case ISD::SETGT:
      std::swap(LHS, RHS);
      break;
    case ISD::SETULE:
    case ISD::SETLE:
      std::swap(LHS, RHS);
      Invert = true;
      break;
    default:
      return SDValue();
    }
    unsigned Opcode = IsSigned ? MCS51ISD::CMPSLT16 : MCS51ISD::CMPULT16;
    SDValue Result = DAG.getNode(Opcode, DL, MVT::i8, LHS, RHS);
    return Invert ? DAG.getNode(ISD::XOR, DL, MVT::i8, Result,
                                DAG.getConstant(1, DL, MVT::i8))
                  : Result;
  };
  if (Op.getOpcode() == ISD::SRL && Op.getValueType() == MVT::i16) {
    auto *Amount = dyn_cast<ConstantSDNode>(Op.getOperand(1));
    if (!Amount || Amount->getZExtValue() != 8)
      report_fatal_error("unsupported MCS-51 16-bit logical shift");
    return DAG.getNode(MCS51ISD::SRL16_8, DL, MVT::i16, Op.getOperand(0));
  }
  if (Op.getOpcode() == ISD::SETCC &&
      Op.getOperand(0).getValueType() == MVT::i16) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
    bool IsSigned = CC == ISD::SETLT || CC == ISD::SETGE ||
                    CC == ISD::SETGT || CC == ISD::SETLE;
    return LowerWordCompare(Op.getOperand(0), Op.getOperand(2), CC, IsSigned);
  }
  if (Op.getOpcode() == ISD::SELECT_CC &&
      Op.getOperand(0).getValueType() == MVT::i16 &&
      Op.getValueType() == MVT::i8) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
    auto *TrueValue = dyn_cast<ConstantSDNode>(Op.getOperand(2));
    auto *FalseValue = dyn_cast<ConstantSDNode>(Op.getOperand(3));
    if (TrueValue && FalseValue &&
        ((TrueValue->isOne() && FalseValue->isZero()) ||
         (TrueValue->isZero() && FalseValue->isOne()))) {
      bool IsSigned = CC == ISD::SETLT || CC == ISD::SETGE ||
                      CC == ISD::SETGT || CC == ISD::SETLE;
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
    bool IsSigned = CC == ISD::SETLT || CC == ISD::SETGE ||
                    CC == ISD::SETGT || CC == ISD::SETLE;
    bool Invert = false;
    switch (CC) {
    case ISD::SETULT:
    case ISD::SETLT:
      break;
    case ISD::SETUGE:
    case ISD::SETGE:
      Invert = true;
      break;
    case ISD::SETUGT:
    case ISD::SETGT:
      std::swap(LHS, RHS);
      break;
    case ISD::SETULE:
    case ISD::SETLE:
      std::swap(LHS, RHS);
      Invert = true;
      break;
    default:
      report_fatal_error("unsupported MCS-51 16-bit branch predicate");
    }
    unsigned CompareOpcode = IsSigned ? MCS51ISD::CMPSLT16
                                      : MCS51ISD::CMPULT16;
    SDValue Result = DAG.getNode(CompareOpcode, DL, MVT::i8, LHS, RHS);
    unsigned BranchOpcode = Invert ? MCS51ISD::BR_EQ : MCS51ISD::BR_NE;
    return DAG.getNode(BranchOpcode, DL, MVT::Other, Op.getOperand(0),
                       Result, DAG.getConstant(0, DL, MVT::i8), Dest);
  }
  if (LHS.getValueType() != MVT::i8 || RHS.getValueType() != MVT::i8)
    report_fatal_error("unsupported MCS-51 conditional branch");

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
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_MCS51);

  for (const CCValAssign &VA : ArgLocs) {
    if (EVT(VA.getLocVT()) != VA.getValVT() ||
        (VA.getValVT() != MVT::i8 && VA.getValVT() != MVT::i16))
      report_fatal_error("unsupported MCS-51 argument type");
    if (VA.isMemLoc()) {
      int64_t SPAdjust = -(VA.getLocMemOffset() + 2);
      SDValue Offset = DAG.getTargetConstant(static_cast<uint8_t>(SPAdjust),
                                             DL, MVT::i8);
      SDValue Ops[] = {Chain, Offset};
      bool IsWord = VA.getValVT() == MVT::i16;
      SDVTList VTs = DAG.getVTList(VA.getValVT(), MVT::Other);
      SDValue Load = DAG.getNode(IsWord ? MCS51ISD::LOAD_STACK16
                                        : MCS51ISD::LOAD_STACK8,
                                 DL, VTs, Ops);
      InVals.push_back(Load);
      Chain = Load.getValue(1);
      continue;
    }
    if (!VA.isRegLoc())
      report_fatal_error("unsupported MCS-51 argument location");
    const TargetRegisterClass *RC = VA.getLocVT() == MVT::i16
                                        ? &MCS51::MCS51PTRRegClass
                                        : &MCS51::MCS51GPR8RegClass;
    Register LiveIn = MF.addLiveIn(VA.getLocReg(),
                                   RC);
    SDValue Value = DAG.getCopyFromReg(Chain, DL, LiveIn, VA.getLocVT());
    if (VA.getLocInfo() != CCValAssign::Full)
      report_fatal_error("unsupported MCS-51 argument extension");
    InVals.push_back(Value);
  }
  return Chain;
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

  for (unsigned I = 0; I < CCInfo.getStackSize(); ++I)
    Chain = DAG.getNode(MCS51ISD::POP_ARG8, DL, MVT::Other, Chain);
  if (CCInfo.getStackSize() != 0)
    InGlue = SDValue();

  if (CLI.RetTy && CLI.RetTy->isIntegerTy(32)) {
    if (CLI.Ins.size() != 4)
      report_fatal_error("unexpected MCS-51 i32 return parts");
    static constexpr Register ReturnRegs[] = {MCS51::R4, MCS51::R5,
                                               MCS51::R6, MCS51::R7};
    for (Register Reg : ReturnRegs) {
      SDValue Part = DAG.getCopyFromReg(Chain, DL, Reg, MVT::i8, InGlue);
      Chain = Part.getValue(1);
      InGlue = Part.getValue(2);
      InVals.push_back(Part.getValue(0));
    }
    return Chain;
  }

  SmallVector<CCValAssign, 2> RetLocs;
  CCState RetInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
  RetInfo.AnalyzeCallResult(CLI.Ins, RetCC_MCS51);
  for (const CCValAssign &VA : RetLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(),
                               InGlue);
    InGlue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }
  return Chain;
}

bool MCS51TargetLowering::CanLowerReturn(
    CallingConv::ID, MachineFunction &, bool,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &,
    const Type *) const {
  return Outs.empty() ||
         (Outs.size() == 1 &&
          (Outs.front().VT == MVT::i8 || Outs.front().VT == MVT::i16)) ||
         (Outs.size() == 4 && Outs.front().ArgVT == MVT::i32 &&
          llvm::all_of(Outs, [](const ISD::OutputArg &Arg) {
            return Arg.VT == MVT::i8;
          }));
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
  if (Outs.size() == 4 && Outs.front().ArgVT == MVT::i32) {
    bool AllConstant = llvm::all_of(OutVals, [](SDValue Value) {
      return isa<ConstantSDNode>(Value);
    });
    if (AllConstant) {
      SmallVector<SDValue, 5> Ops{Chain};
      for (SDValue Value : OutVals)
        Ops.push_back(DAG.getConstant(
            cast<ConstantSDNode>(Value)->getZExtValue(), DL, MVT::i8));
      return DAG.getNode(MCS51ISD::RET_I32_IMM, DL, MVT::Other, Ops);
    }
    SDValue Ops[] = {Chain, OutVals[0], OutVals[1], OutVals[2], OutVals[3]};
    return DAG.getNode(MCS51ISD::RET_I32, DL, MVT::Other, Ops);
  } else if (!OutVals.empty() &&
             (Outs.front().VT == MVT::i8 || Outs.front().Flags.isZExt() ||
              Outs.front().Flags.isSExt())) {
    SDValue RetVal = OutVals.front();
    if (RetVal.getValueType() == MVT::i16)
      RetVal = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, RetVal);
    Chain = DAG.getCopyToReg(Chain, DL, MCS51::A, RetVal);
  } else if (!OutVals.empty() && Outs.front().VT == MVT::i16) {
    SDValue RetVal = OutVals.front();
    if (RetVal.getValueType() == MVT::i8)
      RetVal = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, RetVal);
    Chain = DAG.getCopyToReg(Chain, DL, MCS51::DPTR, RetVal);
  }
  if (!Outs.empty() && Outs.front().VT == MVT::i16 &&
      !Outs.front().Flags.isZExt() && !Outs.front().Flags.isSExt())
    return DAG.getNode(MCS51ISD::RET_WORD, DL, MVT::Other, Chain);
  return DAG.getNode(MCS51ISD::RET_GLUE, DL, MVT::Other, Chain);
}

MachineBasicBlock *MCS51TargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *MBB) const {
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MII = MI.getIterator();
  const DebugLoc &DL = MI.getDebugLoc();
  if (MI.getOpcode() == MCS51::ICALL) {
    MachineFunction &MF = *MBB->getParent();
    MachineBasicBlock *ReturnBB = MBB->splitAt(MI);
    if (ReturnBB == MBB) {
      ReturnBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(++MBB->getIterator(), ReturnBB);
      MBB->addSuccessor(ReturnBB);
    }

    MachineBasicBlock *DispatchBB =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(ReturnBB->getIterator(), DispatchBB);
    DispatchBB->setMachineBlockAddressTaken();
    DispatchBB->addLiveIn(MCS51::A);
    DispatchBB->addLiveIn(MCS51::DPTR);
    // Keep the locally-called dispatcher reachable to machine CFG cleanup.
    MBB->addSuccessor(DispatchBB);

    MI.eraseFromParent();
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::LCALL)).addMBB(DispatchBB);
    BuildMI(*MBB, MBB->end(), DL, TII.get(MCS51::LJMP)).addMBB(ReturnBB);
    BuildMI(*DispatchBB, DispatchBB->end(), DL, TII.get(MCS51::CLR_A));
    BuildMI(*DispatchBB, DispatchBB->end(), DL,
            TII.get(MCS51::JMP_ADPTR));
    return ReturnBB;
  }
  if (MI.getOpcode() == MCS51::LOADIDATA_GLOBAL8) {
    Register Dst = MI.getOperand(0).getReg();
    MachineMemOperand *MMO = MI.memoperands().front();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM))
        .addReg(MCS51::R0, RegState::Define)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(MMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Dst, RegState::Define);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREIDATA_GLOBAL8) {
    MachineMemOperand *MMO = MI.memoperands().front();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))
        .addReg(MI.getOperand(1).getReg());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM))
        .addReg(MCS51::R0, RegState::Define)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(MMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADIDATA_GLOBAL16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineMemOperand *MMO = MI.memoperands().front();
    MachineFunction &MF = *MBB->getParent();
    MachineMemOperand *LowMMO = MF.getMachineMemOperand(MMO, 0, 1);
    MachineMemOperand *HighMMO = MF.getMachineMemOperand(MMO, 1, 1);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM))
        .addReg(MCS51::R0, RegState::Define)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(LowMMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_RN))
        .addReg(MCS51::R0, RegState::Define)
        .addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI))
        .addReg(MCS51::R0)
        .addMemOperand(HighMMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREIDATA_GLOBAL16) {
    MachineMemOperand *MMO = MI.memoperands().front();
    MachineFunction &MF = *MBB->getParent();
    MachineMemOperand *LowMMO = MF.getMachineMemOperand(MMO, 0, 1);
    MachineMemOperand *HighMMO = MF.getMachineMemOperand(MMO, 1, 1);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM))
        .addReg(MCS51::R0, RegState::Define)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(LowMMO);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_RN))
        .addReg(MCS51::R0, RegState::Define)
        .addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_IND_RI_A))
        .addReg(MCS51::R0)
        .addMemOperand(HighMMO);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::ADDDPTR16ri) {
    Register Dst = MI.getOperand(0).getReg();
    int64_t Amount = MI.getOperand(2).getImm();
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .add(MI.getOperand(1));
    for (int64_t I = 0; I < Amount; ++I) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
          .addReg(MCS51::DPTR);
    }
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::ADD16rr ||
      MI.getOpcode() == MCS51::SUB16rr) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    Register LHSLo = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);

    // DPTR is the only allocatable 16-bit register. Keep the left low byte in
    // one temporary and its high byte in B while loading the right operand.
    auto CopyDPTR = [&](Register Src) {
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
          .addReg(Src);
    };
    CopyDPTR(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LHSLo)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_A));
    CopyDPTR(RHS);

    bool IsAdd = MI.getOpcode() == MCS51::ADD16rr;
    if (!IsAdd)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHSLo);
    BuildMI(*MBB, MII, DL,
            TII.get(IsAdd ? MCS51::ADD_A_DIRECT : MCS51::SUBB_A_DIRECT),
            MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPL_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_B), MCS51::A);
    BuildMI(*MBB, MII, DL,
            TII.get(IsAdd ? MCS51::ADDC_A_DIRECT : MCS51::SUBB_A_DIRECT),
            MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPH_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::CMP16) {
    MachineFunction &MF = *MBB->getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    bool IsSigned = MI.getOperand(3).getImm() != 0;
    Register LHSLo = MF.getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    auto CopyDPTR = [&](Register Src) {
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
          .addReg(Src);
    };
    CopyDPTR(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LHSLo)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_A));
    CopyDPTR(RHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHSLo);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_DIRECT), MCS51::A)
        .addImm(0x82);
    if (IsSigned) {
      // Bias both high bytes by 0x80 so unsigned borrow reflects signed order.
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
          .addImm(0x83);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
          .addImm(0x80);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XCH_A_DIRECT), MCS51::A)
          .addImm(0xF0);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::XRL_A_IMM), MCS51::A)
          .addImm(0x80);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_DIRECT), MCS51::A)
          .addImm(0xF0);
    } else {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_B), MCS51::A);
      BuildMI(*MBB, MII, DL, TII.get(MCS51::SUBB_A_DIRECT), MCS51::A)
          .addImm(0x83);
    }
    // CLR A preserves CY, and RLC moves the borrow into bit zero.
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RLC_A));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::RET_I32) {
    static constexpr Register ReturnRegs[] = {MCS51::R4, MCS51::R5,
                                               MCS51::R6, MCS51::R7};
    for (unsigned I = 0; I != 4; ++I)
      BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), ReturnRegs[I])
          .add(MI.getOperand(I));
    MachineInstrBuilder Ret = BuildMI(*MBB, MII, DL, TII.get(MCS51::RET));
    for (Register Reg : ReturnRegs)
      Ret.addReg(Reg, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::RET_I32_IMM) {
    static constexpr Register ReturnRegs[] = {MCS51::R4, MCS51::R5,
                                               MCS51::R6, MCS51::R7};
    for (unsigned I = 0; I != 4; ++I)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_IMM), ReturnRegs[I])
          .add(MI.getOperand(I));
    MachineInstrBuilder Ret = BuildMI(*MBB, MII, DL, TII.get(MCS51::RET));
    for (Register Reg : ReturnRegs)
      Ret.addReg(Reg, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::RET16) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RET))
        .addReg(MCS51::DPTR, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SRL16_8) {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
        .addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::BUILDPAIR16) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))
        .addReg(MI.getOperand(1).getReg());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))
        .addReg(MI.getOperand(2).getReg());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
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
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))).addReg(MCS51::R0);
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::DEC_A)));
    Setup(BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A)))
        .addReg(MCS51::R0, RegState::Define);
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
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83)
        .addReg(MI.getOperand(0).getReg(), RegState::Implicit);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_DIRECT))
        .addImm(0xE0)
        .addReg(MCS51::A, RegState::Implicit);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_DIRECT))
        .addImm(0xE0)
        .addReg(MCS51::A, RegState::Implicit);
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
  if (MI.getOpcode() == MCS51::POPARG8) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::POP_DIRECT))
        .addImm(0xF0)
        .addReg(MCS51::B, RegState::ImplicitDefine);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::INCDPTR16) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
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
      MI.getOpcode() == MCS51::LOADCODE16) {
    Register Dst = MI.getOperand(0).getReg();
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    bool IsCode = MI.getOpcode() == MCS51::LOADCODE16;
    if (IsCode)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL,
            TII.get(IsCode ? MCS51::MOVC_ADPTR : MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    if (IsCode)
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL,
            TII.get(IsCode ? MCS51::MOVC_ADPTR : MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADDIRECT8) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADDIRECT16) {
    Register Dst = MI.getOperand(0).getReg();
    MachineOperand AddrLow = MI.getOperand(1);
    MachineOperand AddrHigh = getNextDirectAddress(AddrLow);
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register HighByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .add(AddrLow);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .add(AddrHigh);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), HighByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(HighByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
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
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREDIRECT8) {
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A))
        .add(MI.getOperand(0));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREXABS8) {
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREXABS16) {
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register HighByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), HighByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DPTR_IMM), MCS51::DPTR)
        .add(MI.getOperand(0));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(HighByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREDIRECT16) {
    MachineOperand AddrLow = MI.getOperand(0);
    MachineOperand AddrHigh = getNextDirectAddress(AddrLow);
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register HighByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), HighByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).add(AddrLow);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(HighByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).add(AddrHigh);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADX8) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADCODE8) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVC_ADPTR));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREX8) {
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREX16) {
    Register Addr = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register HighByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
        .addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), HighByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), MCS51::DPTR)
        .addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_DPTR));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(HighByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOVX_DPTRA));
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADI16 || MI.getOpcode() == MCS51::LOADP16) {
    Register Dst = MI.getOperand(0).getReg();
    Register Addr = MI.getOperand(1).getReg();
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    unsigned LoadOpcode = MI.getOpcode() == MCS51::LOADI16
                              ? MCS51::MOV_A_IND_RI
                              : MCS51::MOVX_A_IND_RI;
    BuildMI(*MBB, MII, DL, TII.get(LoadOpcode)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Addr, RegState::Define);
    BuildMI(*MBB, MII, DL, TII.get(LoadOpcode)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::DEC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Addr, RegState::Define);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::STOREI16 || MI.getOpcode() == MCS51::STOREP16) {
    Register Addr = MI.getOperand(0).getReg();
    Register LowByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    Register HighByte = MBB->getParent()->getRegInfo().createVirtualRegister(
        &MCS51::MCS51GPR8RegClass);
    unsigned StoreOpcode = MI.getOpcode() == MCS51::STOREI16
                               ? MCS51::MOV_IND_RI_A
                               : MCS51::MOVX_IND_RI_A;
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), LowByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x83);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), HighByte)
        .addReg(MCS51::A);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LowByte);
    BuildMI(*MBB, MII, DL, TII.get(StoreOpcode)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::INC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(Addr, RegState::Define);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(HighByte);
    BuildMI(*MBB, MII, DL, TII.get(StoreOpcode)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Addr);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::DEC_A));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A)).addReg(Addr);
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
  if (MI.getOpcode() == MCS51::TRUNC16TO8) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::ZEXT8TO16) {
    // DPTR is exposed as one i16 register, while its byte halves are SFRs.
    // Clear DPH and write the source byte to DPL through the accumulator.
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_IMM))
        .addImm(0x83)
        .addImm(0);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A)).addImm(0x82);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::DPTR);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::SHL8ri || MI.getOpcode() == MCS51::SRL8ri) {
    unsigned Amount = MI.getOperand(2).getImm();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    if (Amount >= 8) {
      BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_A));
    } else {
      unsigned RotateOpcode = MI.getOpcode() == MCS51::SHL8ri
                                  ? MCS51::RLC_A
                                  : MCS51::RRC_A;
      while (Amount--) {
        BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
        BuildMI(*MBB, MII, DL, TII.get(RotateOpcode));
      }
    }
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
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

  BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
  if (IsSubtraction)
    BuildMI(*MBB, MII, DL, TII.get(MCS51::CLR_C));
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
    AccOpcode = MCS51::XRL_A_IMM;
    break;
  default:
    llvm_unreachable("unexpected MCS-51 ALU pseudo");
  }
  if (IsImmediate) {
    BuildMI(*MBB, MII, DL, TII.get(AccOpcode), MCS51::A)
        .addImm(MI.getOperand(2).getImm());
  } else {
    BuildMI(*MBB, MII, DL, TII.get(AccOpcode))
        .addReg(MI.getOperand(2).getReg());
  }
  BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
      .addReg(MCS51::A);
  MI.eraseFromParent();
  return MBB;
}
