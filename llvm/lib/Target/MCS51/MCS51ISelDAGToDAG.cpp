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
    if (LD->getAddressSpace() != MCS51::XData)
      return false;
    if (LD->getMemoryVT() != MVT::i8)
      report_fatal_error("MCS-51 XDATA currently supports byte loads only");
    SDValue Ops[] = {LD->getBasePtr(), LD->getChain()};
    SDNode *Res = CurDAG->getMachineNode(MCS51::LOADX8, DL, N->getVTList(),
                                         Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
    CurDAG->RemoveDeadNode(N);
    return true;
  }

  auto *ST = cast<StoreSDNode>(N);
  if (ST->getAddressSpace() != MCS51::XData)
    return false;
  if (ST->getMemoryVT() != MVT::i8)
    report_fatal_error("MCS-51 XDATA currently supports byte stores only");
  SDValue Ops[] = {ST->getBasePtr(), ST->getValue(), ST->getChain()};
  SDNode *Res = CurDAG->getMachineNode(MCS51::STOREX8, DL, MVT::Other, Ops);
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
