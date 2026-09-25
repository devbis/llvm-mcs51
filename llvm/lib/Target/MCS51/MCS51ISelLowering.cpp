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
  setBooleanContents(ZeroOrOneBooleanContent);
  setStackPointerRegisterToSaveRestore(MCS51::SP);
  computeRegisterProperties(STI.getRegisterInfo());
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

bool MCS51TargetLowering::CanLowerReturn(
    CallingConv::ID, MachineFunction &, bool,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &,
    const Type *) const {
  return Outs.empty() ||
         (Outs.size() == 1 && Outs.front().VT == MVT::i8);
}

SDValue MCS51TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID, bool,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  if (Outs.empty() != OutVals.empty())
    report_fatal_error("MCS-51 return value lowering mismatch");
  if (OutVals.size() > 1 ||
      (!OutVals.empty() && OutVals.front().getValueType() != MVT::i8))
    report_fatal_error("MCS-51 value return lowering is not implemented");
  if (!OutVals.empty())
    Chain = DAG.getCopyToReg(Chain, DL, MCS51::A, OutVals.front());
  return DAG.getNode(MCS51ISD::RET_GLUE, DL, MVT::Other, Chain);
}

MachineBasicBlock *MCS51TargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *MBB) const {
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MII = MI.getIterator();
  const DebugLoc &DL = MI.getDebugLoc();
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  unsigned AccOpcode;
  bool IsImmediate = MI.getOpcode() == MCS51::ADD8ri ||
                     MI.getOpcode() == MCS51::AND8ri ||
                     MI.getOpcode() == MCS51::OR8ri ||
                     MI.getOpcode() == MCS51::XOR8ri;

  BuildMI(*MBB, MII, DL, TII.get(MCS51::MOV_A_RN)).addReg(LHS);
  switch (MI.getOpcode()) {
  case MCS51::ADD8rr:
    AccOpcode = MCS51::ADD_A_RN;
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
