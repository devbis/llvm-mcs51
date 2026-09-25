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
  setOperationAction(ISD::SHL, MVT::i8, Legal);
  setOperationAction(ISD::SRL, MVT::i8, Legal);
  setBooleanContents(ZeroOrOneBooleanContent);
  setStackPointerRegisterToSaveRestore(MCS51::SP);
  computeRegisterProperties(STI.getRegisterInfo());
}

EVT MCS51TargetLowering::getSetCCResultType(const DataLayout &, LLVMContext &,
                                            EVT VT) const {
  assert(!VT.isVector() && "MCS-51 does not support vector comparisons");
  return MVT::i8;
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
    if (!VA.isRegLoc())
      report_fatal_error("MCS-51 stack arguments are not implemented");
    if (VA.getLocVT() != MVT::i8 || VA.getValVT() != MVT::i8)
      report_fatal_error("unsupported MCS-51 argument type");
    Register LiveIn = MF.addLiveIn(VA.getLocReg(),
                                   &MCS51::MCS51GPR8RegClass);
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
  if (CCInfo.getStackSize() != 0)
    report_fatal_error("MCS-51 stack arguments are not implemented");

  SDValue Chain = CLI.Chain;
  SDValue InGlue;
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  for (unsigned I = 0; I < ArgLocs.size(); ++I) {
    const CCValAssign &VA = ArgLocs[I];
    if (!VA.isRegLoc() || VA.getLocVT() != MVT::i8 ||
        CLI.OutVals[I].getValueType() != MVT::i8)
      report_fatal_error("unsupported MCS-51 call argument");
    RegsToPass.emplace_back(VA.getLocReg(), CLI.OutVals[I]);
  }

  for (const auto &[Reg, Value] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Value, InGlue);
    InGlue = Chain.getValue(1);
  }

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
  if (!OutVals.empty())
    Chain = DAG.getCopyToReg(Chain, DL,
                             OutVals.front().getValueType() == MVT::i8
                                 ? MCS51::A
                                 : MCS51::DPTR,
                             OutVals.front());
  return DAG.getNode(MCS51ISD::RET_GLUE, DL, MVT::Other, Chain);
}

MachineBasicBlock *MCS51TargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *MBB) const {
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MII = MI.getIterator();
  const DebugLoc &DL = MI.getDebugLoc();
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
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
