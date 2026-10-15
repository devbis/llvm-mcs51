#ifndef LLVM_LIB_TARGET_MCS51_MCS51MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/ADT/DenseMap.h"
#include <array>
#include <utility>

namespace llvm {

class GlobalVariable;

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
  // Frame objects moved out of the hardware stack into the function's static
  // frame (a PDATA object), with their byte offset in that frame.
  DenseMap<int, unsigned> StaticFrameOffsets;
  GlobalVariable *StaticFrame = nullptr;

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
  GlobalVariable *getStaticFrame() const { return StaticFrame; }
  void setStaticFrame(GlobalVariable *GV) { StaticFrame = GV; }
  const unsigned *getStaticFrameOffset(int FI) const {
    auto It = StaticFrameOffsets.find(FI);
    return It == StaticFrameOffsets.end() ? nullptr : &It->second;
  }
  void setStaticFrameOffset(int FI, unsigned Offset) {
    StaticFrameOffsets[FI] = Offset;
  }

  void setDptrAddress(const MachineInstr *MI, Register Base, int64_t Offset) {
    DptrAddresses[MI] = {Base, Offset};
  }
};

} // namespace llvm

#endif
