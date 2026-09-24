#ifndef LLVM_LIB_TARGET_MCS51_MCTARGETDESC_MCS51MCASMINFO_H
#define LLVM_LIB_TARGET_MCS51_MCTARGETDESC_MCS51MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class MCS51MCAsmInfo : public MCAsmInfoELF {
public:
  explicit MCS51MCAsmInfo(const Triple &TT, const MCTargetOptions &Options);

private:
  void anchor() override;
};
} // namespace llvm

#endif
