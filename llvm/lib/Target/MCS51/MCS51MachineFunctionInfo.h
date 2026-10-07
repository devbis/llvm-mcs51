#ifndef LLVM_LIB_TARGET_MCS51_MCS51MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/ADT/DenseMap.h"
#include <array>
#include <utility>

namespace llvm {

class MCS51MachineFunctionInfo final : public MachineFunctionInfo {
public:
  struct CallFrameStateAtEntry {
    bool Active = false;
    // Stack adjustment from the SP value at the start of the call sequence.
    // Stack arguments are pushed explicitly, so this can differ from the
    // complete logical call frame while argument values are being prepared.
    int64_t SPAdj = 0;
  };

private:
  int VarArgsFrameIndex = 0;
  bool HasVarArgsFrameIndex = false;
  DenseMap<Register, std::array<Register, 2>> CompareByteCaptures;
  DenseMap<const MachineBasicBlock *, CallFrameStateAtEntry>
      CallFrameStatesAtEntry;
  // DPTR-defining instructions emitted for an address: the word it was built
  // from and the offset it holds right after that instruction.
  DenseMap<const MachineInstr *, std::pair<Register, int64_t>> DptrAddresses;

public:
  MCS51MachineFunctionInfo(const Function &, const TargetSubtargetInfo *) {}

  const CallFrameStateAtEntry *getCallFrameStateAtEntry(
      const MachineBasicBlock *MBB) const {
    auto It = CallFrameStatesAtEntry.find(MBB);
    return It == CallFrameStatesAtEntry.end() ? nullptr : &It->second;
  }

  void setCallFrameStateAtEntry(
      const MachineBasicBlock *MBB, CallFrameStateAtEntry State) {
    CallFrameStatesAtEntry[MBB] = State;
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  bool hasVarArgsFrameIndex() const { return HasVarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) {
    VarArgsFrameIndex = Index;
    HasVarArgsFrameIndex = true;
  }

  const std::array<Register, 2> *getCompareByteCaptures(Register Word) const {
    auto It = CompareByteCaptures.find(Word);
    return It == CompareByteCaptures.end() ? nullptr : &It->second;
  }

  void setCompareByteCaptures(Register Word,
                              std::array<Register, 2> Bytes) {
    CompareByteCaptures[Word] = Bytes;
  }

  const std::pair<Register, int64_t> *
  getDptrAddress(const MachineInstr *MI) const {
    auto It = DptrAddresses.find(MI);
    return It == DptrAddresses.end() ? nullptr : &It->second;
  }
  void setDptrAddress(const MachineInstr *MI, Register Base, int64_t Offset) {
    DptrAddresses[MI] = {Base, Offset};
  }
};

} // namespace llvm

#endif
