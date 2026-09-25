#ifndef LLVM_LIB_TARGET_MCS51_MCS51TARGETMACHINE_H
#define LLVM_LIB_TARGET_MCS51_MCS51TARGETMACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include <optional>

namespace llvm {

class MCS51TargetMachine final : public CodeGenTargetMachineImpl {
public:
  MCS51TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                     StringRef FS, const TargetOptions &Options,
                     std::optional<Reloc::Model> RM,
                     std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                     bool JIT);

  bool addPassesToEmitFile(PassManagerBase &PM, raw_pwrite_stream &Out,
                           raw_pwrite_stream *DwoOut, CodeGenFileType FileType,
                           bool DisableVerify = true,
                           MachineModuleInfoWrapperPass *MMIWP = nullptr)
      override;
};

} // namespace llvm

#endif
