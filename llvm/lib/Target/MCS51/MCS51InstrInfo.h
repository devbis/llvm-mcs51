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
  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI, Register SrcReg,
                           bool IsKill, int FrameIndex,
                           const TargetRegisterClass *RC, Register VReg,
                           MachineInstr::MIFlag Flags) const override;
  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI, Register DestReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            Register VReg, unsigned SubReg,
                            MachineInstr::MIFlag Flags) const override;
  bool expandPostRAPseudo(MachineInstr &MI) const override;
  int getSPAdjust(const MachineInstr &MI) const override;
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify = false) const override;
  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB,
                        ArrayRef<MachineOperand> Cond, const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;
  bool reverseBranchCondition(
      SmallVectorImpl<MachineOperand> &Cond) const override;
  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;
  MachineBasicBlock *getBranchDestBlock(const MachineInstr &MI) const override;
  bool isBranchOffsetInRange(unsigned BranchOpc,
                             int64_t BrOffset) const override;
  void insertIndirectBranch(MachineBasicBlock &MBB,
                            MachineBasicBlock &NewDestBB,
                            MachineBasicBlock &RestoreBB,
                            const DebugLoc &DL, int64_t BrOffset,
                            RegScavenger *RS = nullptr) const override;

private:
  const MCS51RegisterInfo RI;
};

} // namespace llvm

#endif
