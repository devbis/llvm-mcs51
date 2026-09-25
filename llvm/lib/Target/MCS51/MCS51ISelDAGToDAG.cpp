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
      if (LD->getMemoryVT() != MVT::i8)
        report_fatal_error("MCS-51 direct memory supports byte loads only");
      SDValue Addr = LD->getBasePtr();
      if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
        Addr = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
      else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
        Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
      else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
        return false;
      SDValue Ops[] = {Addr, LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOADDIRECT8, DL,
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
    if (ST->getMemoryVT() != MVT::i8)
      report_fatal_error("MCS-51 direct memory supports byte stores only");
    SDValue Addr = ST->getBasePtr();
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
      Addr = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
    else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
      Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
    else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
      return false;
    SDValue Ops[] = {Addr, ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(MCS51::STOREDIRECT8, DL,
                                         MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  if (AS != MCS51::IData && AS != MCS51::PData && AS != MCS51::XData)
    return false;
  if (ST->getMemoryVT() != MVT::i8)
    report_fatal_error("MCS-51 data spaces currently support byte stores only");
  unsigned Opcode = AS == MCS51::IData ? MCS51::STOREI8 :
                    AS == MCS51::PData ? MCS51::STOREP8 : MCS51::STOREX8;
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
