#ifndef LLVM_LIB_TARGET_MCS51_MCS51SUBTARGET_H
#define LLVM_LIB_TARGET_MCS51_MCS51SUBTARGET_H

#include "MCS51FrameLowering.h"
#include "MCS51InstrInfo.h"
#include "MCS51ISelLowering.h"
#include "MCS51SelectionDAGInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

#define GET_SUBTARGETINFO_HEADER
#include "MCS51GenSubtargetInfo.inc"

namespace llvm {

class MCS51Subtarget final : public MCS51GenSubtargetInfo {
public:
  MCS51Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                 const TargetMachine &TM);

  const MCS51InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const MCS51TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const MCS51SelectionDAGInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
  const MCS51RegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }

  void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU,
                              StringRef FS);

private:
  MCS51InstrInfo InstrInfo;
  MCS51FrameLowering FrameLowering;
  MCS51TargetLowering TLInfo;
  MCS51SelectionDAGInfo TSInfo;
};

} // namespace llvm

#endif
