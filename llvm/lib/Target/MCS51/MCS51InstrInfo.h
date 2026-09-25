#ifndef LLVM_LIB_TARGET_MCS51_MCS51INSTRINFO_H
#define LLVM_LIB_TARGET_MCS51_MCS51INSTRINFO_H

#include "MCS51RegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "MCS51GenInstrInfo.inc"

namespace llvm {

class MCS51Subtarget;

class MCS51InstrInfo final : public MCS51GenInstrInfo {
public:
  explicit MCS51InstrInfo(const MCS51Subtarget &STI);
  const MCS51RegisterInfo &getRegisterInfo() const { return RI; }
  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

private:
  const MCS51RegisterInfo RI;
};

} // namespace llvm

#endif
