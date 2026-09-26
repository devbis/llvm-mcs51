#include "MCS51TargetMachine.h"
#include "MCS51.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCSectionELF.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {
class MCS51TargetObjectFile final : public TargetLoweringObjectFileELF {
public:
  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override {
    const auto *GV = dyn_cast<GlobalVariable>(GO);
    if (GV && !GV->hasSection() &&
        (GV->getAddressSpace() == MCS51::Data ||
         GV->getAddressSpace() == MCS51::IData)) {
      bool IsData = GV->getAddressSpace() == MCS51::Data;
      StringRef Name = IsData ? (Kind.isBSS() ? ".mcs51.data1.bss"
                                              : ".mcs51.data1")
                              : (Kind.isBSS() ? ".mcs51.data2.bss"
                                              : ".mcs51.data2");
      unsigned Type = Kind.isBSS() ? ELF::SHT_NOBITS : ELF::SHT_PROGBITS;
      unsigned Flags = ELF::SHF_ALLOC;
      if (!Kind.isReadOnly())
        Flags |= ELF::SHF_WRITE;
      return getContext().getELFSection(Name, Type, Flags);
    }
    return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
  }
};

constexpr StringLiteral MCS51DataLayout =
    "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-"
    "i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16";
}

namespace {
class MCS51PassConfig final : public TargetPassConfig {
public:
  MCS51PassConfig(MCS51TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  MCS51TargetMachine &getMCS51TargetMachine() const {
    return getTM<MCS51TargetMachine>();
  }

  bool addInstSelector() override {
    addPass(createMCS51ISelDag(getMCS51TargetMachine(), getOptLevel()));
    return false;
  }

  void addPreEmitPass() override { addPass(&BranchRelaxationPassID); }
};
} // namespace

MCS51TargetMachine::MCS51TargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, MCS51DataLayout, TT, CPU, FS, Options,
          RM.value_or(Reloc::Static),
          getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<MCS51TargetObjectFile>()),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

bool MCS51TargetMachine::addPassesToEmitFile(
    PassManagerBase &PM, raw_pwrite_stream &Out, raw_pwrite_stream *DwoOut,
    CodeGenFileType FileType, bool DisableVerify,
    MachineModuleInfoWrapperPass *MMIWP) {
  return CodeGenTargetMachineImpl::addPassesToEmitFile(
      PM, Out, DwoOut, FileType, DisableVerify, MMIWP);
}

TargetPassConfig *MCS51TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new MCS51PassConfig(*this, PM);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMCS51Target() {
  RegisterTargetMachine<MCS51TargetMachine> X(getTheMCS51Target());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeMCS51AsmPrinterPass(PR);
  initializeMCS51DAGToDAGISelLegacyPass(PR);
}
