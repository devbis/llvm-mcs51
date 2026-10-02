#include "MCS51MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void MCS51MCAsmInfo::anchor() {}

MCS51MCAsmInfo::MCS51MCAsmInfo(const Triple &, const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  CodePointerSize = 2;
  CalleeSaveStackSlotSize = 1;
  CommentString = ";";
  MaxInstLength = 3;
  AlignmentIsInBytes = true;
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::None;
}
