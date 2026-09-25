#include "MCS51ISelLowering.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/Support/ErrorHandling.h"

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
  setOperationAction(ISD::BR_CC, MVT::i8, Custom);
  setBooleanContents(ZeroOrOneBooleanContent);
  setStackPointerRegisterToSaveRestore(MCS51::SP);
  computeRegisterProperties(STI.getRegisterInfo());
}

EVT MCS51TargetLowering::getSetCCResultType(const DataLayout &, LLVMContext &,
                                            EVT VT) const {
  assert(!VT.isVector() && "MCS-51 does not support vector comparisons");
  return MVT::i8;
}

SDValue MCS51TargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  if (Op.getOpcode() != ISD::BR_CC)
    return SDValue();

  SDLoc DL(Op);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
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
      if (VA.getValVT() != MVT::i8)
        report_fatal_error("MCS-51 stack arguments currently support i8 only");
      int64_t SPAdjust = -(VA.getLocMemOffset() + 2);
      SDValue Offset = DAG.getTargetConstant(static_cast<uint8_t>(SPAdjust),
                                             DL, MVT::i8);
      SDValue Ops[] = {Chain, Offset};
      SDVTList VTs = DAG.getVTList(MVT::i8, MVT::Other);
      SDValue Load = DAG.getNode(MCS51ISD::LOAD_STACK8, DL, VTs, Ops);
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
    else if (VA.isMemLoc() && VT == MVT::i8)
      StackArgs.emplace_back(VA.getLocMemOffset(), CLI.OutVals[I]);
    else
      report_fatal_error("unsupported MCS-51 call argument location");
  }

  // Preserve stack argument values before register argument copies can
  // overwrite the physical registers that currently hold those values.
  for (auto I = StackArgs.rbegin(), E = StackArgs.rend(); I != E; ++I) {
    SDValue Ops[] = {Chain, I->second};
    Chain = DAG.getNode(MCS51ISD::PUSH_ARG8, DL, MVT::Other, Ops);
  }

  for (const auto &[Reg, Value] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Value, InGlue);
    InGlue = Chain.getValue(1);
  }
  if (!StackArgs.empty())
    InGlue = SDValue();

  SDValue Callee = CLI.Callee;
  if (auto *GA = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i16);
  else if (auto *ES = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(ES->getSymbol(), MVT::i16);
  else
    report_fatal_error("unsupported MCS-51 indirect call");

  SmallVector<SDValue, 12> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (const auto &[Reg, Value] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, Value.getValueType()));
  if (InGlue.getNode())
    Ops.push_back(InGlue);
  SDVTList CallVTs = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(MCS51ISD::CALL, DL, CallVTs, Ops);
  InGlue = Chain.getValue(1);

  for (unsigned I = 0; I < CCInfo.getStackSize(); ++I)
    Chain = DAG.getNode(MCS51ISD::POP_ARG8, DL, MVT::Other, Chain);
  if (CCInfo.getStackSize() != 0)
    InGlue = SDValue();

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
          (Outs.front().VT == MVT::i8 || Outs.front().VT == MVT::i16));
}

SDValue MCS51TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID, bool,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  if (Outs.empty() != OutVals.empty())
    report_fatal_error("MCS-51 return value lowering mismatch");
  if (OutVals.size() > 1 ||
      (!OutVals.empty() && OutVals.front().getValueType() != MVT::i8 &&
       OutVals.front().getValueType() != MVT::i16))
    report_fatal_error("MCS-51 value return lowering is not implemented");
  if (!OutVals.empty()) {
    SDValue RetVal = OutVals.front();
    if (Outs.front().VT == MVT::i8 || Outs.front().Flags.isZExt() ||
        Outs.front().Flags.isSExt()) {
      if (RetVal.getValueType() == MVT::i16)
        RetVal = DAG.getNode(ISD::TRUNCATE, DL, MVT::i8, RetVal);
      Chain = DAG.getCopyToReg(Chain, DL, MCS51::A, RetVal);
    } else if (Outs.front().VT == MVT::i16) {
      if (RetVal.getValueType() == MVT::i8)
        RetVal = DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i16, RetVal);
      Chain = DAG.getCopyToReg(Chain, DL, MCS51::DPTR, RetVal);
    } else {
      report_fatal_error("unsupported MCS-51 return type");
    }
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
  if (MI.getOpcode() == MCS51::RET16) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::RET))
        .addReg(MCS51::DPTR, RegState::Implicit);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::LOADSTACKARG8) {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_DIRECT), MCS51::A)
        .addImm(0x81);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::ADD_A_IMM), MCS51::A)
        .add(MI.getOperand(1));
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_RN_A))
        .addReg(MCS51::R0, RegState::Define);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_IND_RI)).addReg(MCS51::R0);
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::PUSHARG8) {
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN))
        .addReg(MI.getOperand(0).getReg());
    BuildMI(*MBB, MII, DL, TII.get(MCS51::PUSH_DIRECT)).addImm(0xE0);
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
  if (MI.getOpcode() == MCS51::STOREDIRECT8) {
    Register Src = MI.getOperand(1).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(Src);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_DIRECT_A))
        .add(MI.getOperand(0));
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
      MI.getOpcode() == MCS51::BR_UGE8rr) {
    Register LHS = MI.getOperand(0).getReg();
    const MachineOperand &RHS = MI.getOperand(1);
    MachineBasicBlock *Target = MI.getOperand(2).getMBB();
    bool IsEqual = MI.getOpcode() == MCS51::BR_EQ8ri ||
                   MI.getOpcode() == MCS51::BR_EQ8rr;
    bool IsNotEqual = MI.getOpcode() == MCS51::BR_NE8ri ||
                      MI.getOpcode() == MCS51::BR_NE8rr;
    bool IsUnsignedLess = MI.getOpcode() == MCS51::BR_ULT8ri ||
                          MI.getOpcode() == MCS51::BR_ULT8rr;
    if (!IsEqual && !IsNotEqual) {
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
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_RN)).addReg(RHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MUL_AB));
    BuildMI(*MBB, MII, DL, TII.get(TargetOpcode::COPY), Dst)
        .addReg(MCS51::A);
    MI.eraseFromParent();
    return MBB;
  }
  if (MI.getOpcode() == MCS51::UDIV8rr || MI.getOpcode() == MCS51::UREM8rr) {
    Register RHS = MI.getOperand(2).getReg();
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
    BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_B_RN)).addReg(RHS);
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
