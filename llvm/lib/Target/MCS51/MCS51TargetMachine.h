#ifndef LLVM_LIB_TARGET_MCS51_MCS51TARGETMACHINE_H
#define LLVM_LIB_TARGET_MCS51_MCS51TARGETMACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "MCS51Subtarget.h"
#include <memory>
#include <optional>

namespace llvm {

class MCS51TargetMachine final : public CodeGenTargetMachineImpl {
public:
  MCS51TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                     StringRef FS, const TargetOptions &Options,
                     std::optional<Reloc::Model> RM,
                     std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                     bool JIT);

  const MCS51Subtarget *getSubtargetImpl() const { return &Subtarget; }
  const MCS51Subtarget *
  getSubtargetImpl(const Function &) const override { return &Subtarget; }

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  bool addPassesToEmitFile(PassManagerBase &PM, raw_pwrite_stream &Out,
                          raw_pwrite_stream *DwoOut, CodeGenFileType FileType,
                          bool DisableVerify = true,
                          MachineModuleInfoWrapperPass *MMIWP = nullptr)
      override;

private:
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  MCS51Subtarget Subtarget;
};

FunctionPass *createMCS51ISelDag(MCS51TargetMachine &TM, CodeGenOptLevel OL);
FunctionPass *createMCS51GenericPointerLoweringPass();
FunctionPass *createMCS51StackAddressLoweringPass();
ModulePass *createMCS51OverlayPass();
void initializeMCS51AsmPrinterPass(PassRegistry &PR);
void initializeMCS51DAGToDAGISelLegacyPass(PassRegistry &PR);

} // namespace llvm

#endif
