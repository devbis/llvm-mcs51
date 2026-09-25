#include "MCS51TargetMachine.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/Compiler.h"

#define DEBUG_TYPE "mcs51-isel"

using namespace llvm;

namespace {
class MCS51DAGToDAGISel final : public SelectionDAGISel {
public:
  MCS51DAGToDAGISel(MCS51TargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISel(TM, OL) {}

  void SelectCode(SDNode *N);
  bool CheckNodePredicate(SDValue Op, unsigned PredNo) const override;
  bool selectXDataMemory(SDNode *N);
  void Select(SDNode *N) override {
    SDLoc DL(N);
    if (N->getOpcode() == MCS51ISD::LOAD_STACK8) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOADSTACKARG8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::LOAD_STACK16) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOADSTACKARG16, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::PUSH_ARG8) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::PUSHARG8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::PUSH_ARG16) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::PUSHARG16, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::POP_ARG8) {
      SDValue Ops[] = {N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::POPARG8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::PUSH_PAD8) {
      SDValue Ops[] = {N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::PUSHPAD8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if ((N->getOpcode() == ISD::LOAD || N->getOpcode() == ISD::STORE) &&
        selectXDataMemory(N))
      return;
    SelectCode(N);
  }
};

bool MCS51DAGToDAGISel::selectXDataMemory(SDNode *N) {
  SDLoc DL(N);
  if (N->getOpcode() == ISD::LOAD) {
    auto *LD = cast<LoadSDNode>(N);
    unsigned AS = LD->getAddressSpace();
    if (AS == MCS51::Data || AS == MCS51::SFR) {
      unsigned Opcode;
      if (LD->getMemoryVT() == MVT::i8)
        Opcode = MCS51::LOADDIRECT8;
      else if (LD->getMemoryVT() == MVT::i16)
        Opcode = MCS51::LOADDIRECT16;
      else
        report_fatal_error("unsupported MCS-51 direct memory load width");
      SDValue Addr = LD->getBasePtr();
      if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
        Addr = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
      else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
        Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
      else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
        return false;
      SDValue Ops[] = {Addr, LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(Opcode, DL,
                                           N->getVTList(), Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if (AS != MCS51::IData && AS != MCS51::PData &&
        AS != MCS51::XData && AS != MCS51::Code)
      return false;
    unsigned Opcode;
    if (LD->getMemoryVT() == MVT::i8) {
      Opcode = AS == MCS51::IData ? MCS51::LOADI8 :
               AS == MCS51::PData ? MCS51::LOADP8 :
               AS == MCS51::Code ? MCS51::LOADCODE8 : MCS51::LOADX8;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::IData) {
      Opcode = MCS51::LOADI16;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::PData) {
      Opcode = MCS51::LOADP16;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::XData) {
      Opcode = MCS51::LOADX16;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::Code) {
      Opcode = MCS51::LOADCODE16;
    } else {
      report_fatal_error("unsupported MCS-51 memory load width/address space");
    }
    SDValue Ops[] = {LD->getBasePtr(), LD->getChain()};
    SDNode *Res = CurDAG->getMachineNode(Opcode, DL, N->getVTList(), Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
    CurDAG->RemoveDeadNode(N);
    return true;
  }

  auto *ST = cast<StoreSDNode>(N);
  unsigned AS = ST->getAddressSpace();
  if (AS == MCS51::Code)
    report_fatal_error("cannot store to MCS-51 code memory");
  if (AS == MCS51::Data || AS == MCS51::SFR) {
    unsigned Opcode;
    if (ST->getMemoryVT() == MVT::i8)
      Opcode = MCS51::STOREDIRECT8;
    else if (ST->getMemoryVT() == MVT::i16)
      Opcode = MCS51::STOREDIRECT16;
    else
      report_fatal_error("unsupported MCS-51 direct memory store width");
    SDValue Addr = ST->getBasePtr();
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
      Addr = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
    else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
      Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
    else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
      return false;
    SDValue Ops[] = {Addr, ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(Opcode, DL,
                                         MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  if (AS != MCS51::IData && AS != MCS51::PData && AS != MCS51::XData)
    return false;
  unsigned Opcode;
  if (ST->getMemoryVT() == MVT::i8)
    Opcode = AS == MCS51::IData ? MCS51::STOREI8 :
             AS == MCS51::PData ? MCS51::STOREP8 : MCS51::STOREX8;
  else if (ST->getMemoryVT() == MVT::i16 && AS == MCS51::IData)
    Opcode = MCS51::STOREI16;
  else if (ST->getMemoryVT() == MVT::i16 && AS == MCS51::PData)
    Opcode = MCS51::STOREP16;
  else
    report_fatal_error("unsupported MCS-51 data memory store width");
  if (AS == MCS51::XData) {
    SDValue Ops[] = {ST->getBasePtr(), ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  SDValue Ops[] = {ST->getBasePtr(), ST->getValue(), ST->getChain()};
  SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
  CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
  ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
  CurDAG->RemoveDeadNode(N);
  return true;
}

class MCS51DAGToDAGISelLegacy final : public SelectionDAGISelLegacy {
public:
  static char ID;
  MCS51DAGToDAGISelLegacy(MCS51TargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<MCS51DAGToDAGISel>(TM, OL)) {}
};
} // namespace

#define GET_DAGISEL_BODY MCS51DAGToDAGISel
#include "MCS51GenDAGISel.inc"

char MCS51DAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(MCS51DAGToDAGISelLegacy, "mcs51-isel",
                "MCS-51 DAG->DAG Instruction Selection", false, false)

FunctionPass *llvm::createMCS51ISelDag(MCS51TargetMachine &TM,
                                       CodeGenOptLevel OL) {
  return new MCS51DAGToDAGISelLegacy(TM, OL);
}
