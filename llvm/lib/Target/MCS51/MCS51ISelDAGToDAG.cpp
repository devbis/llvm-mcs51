#include "MCS51TargetMachine.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
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
  void Select(SDNode *N) override { SelectCode(N); }
};

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
