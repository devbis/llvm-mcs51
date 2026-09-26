#ifndef LLVM_LIB_TARGET_MCS51_MCS51MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MCS51MachineFunctionInfo final : public MachineFunctionInfo {
  int VarArgsFrameIndex = 0;
  bool HasVarArgsFrameIndex = false;

public:
  MCS51MachineFunctionInfo(const Function &, const TargetSubtargetInfo *) {}

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  bool hasVarArgsFrameIndex() const { return HasVarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) {
    VarArgsFrameIndex = Index;
    HasVarArgsFrameIndex = true;
  }
};

} // namespace llvm

#endif
