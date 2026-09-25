#ifndef LLVM_LIB_TARGET_MCS51_MCS51SELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51SELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

#define GET_SDNODE_ENUM
#include "MCS51GenSDNodeInfo.inc"

namespace llvm {

class MCS51SelectionDAGInfo final : public SelectionDAGGenTargetInfo {
public:
  MCS51SelectionDAGInfo();
  ~MCS51SelectionDAGInfo() override;
};

} // namespace llvm

#endif
