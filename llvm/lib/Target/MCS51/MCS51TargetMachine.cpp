#include "MCS51TargetMachine.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {
constexpr StringLiteral MCS51DataLayout =
    "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-"
    "i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16";
}

MCS51TargetMachine::MCS51TargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, MCS51DataLayout, TT, CPU, FS, Options,
          RM.value_or(Reloc::Static),
          getEffectiveCodeModel(CM, CodeModel::Small), OL) {
  initAsmInfo();
}

bool MCS51TargetMachine::addPassesToEmitFile(
    PassManagerBase &, raw_pwrite_stream &, raw_pwrite_stream *,
    CodeGenFileType, bool, MachineModuleInfoWrapperPass *) {
  // The SelectionDAG lowering and assembly printer are added with the MCS-51
  // code generator. Do not silently emit an empty output file in the meantime.
  return true;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMCS51Target() {
  RegisterTargetMachine<MCS51TargetMachine> X(getTheMCS51Target());
}
