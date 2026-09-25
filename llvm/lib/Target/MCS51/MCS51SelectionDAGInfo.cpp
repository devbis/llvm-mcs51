#include "MCS51SelectionDAGInfo.h"

#define GET_SDNODE_DESC
#include "MCS51GenSDNodeInfo.inc"

using namespace llvm;

MCS51SelectionDAGInfo::MCS51SelectionDAGInfo()
    : SelectionDAGGenTargetInfo(MCS51GenSDNodeInfo) {}

MCS51SelectionDAGInfo::~MCS51SelectionDAGInfo() = default;
