#include "MCS51TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
Target &getTheMCS51Target() {
  static Target TheMCS51Target;
  return TheMCS51Target;
}
} // namespace llvm

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMCS51TargetInfo() {
  llvm::RegisterTarget<llvm::Triple::mcs51> X(
      llvm::getTheMCS51Target(), "mcs51", "MCS-51 8-bit microcontrollers",
      "MCS51");
}
