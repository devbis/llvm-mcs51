#include "MCS51Subtarget.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "MCS51GenSubtargetInfo.inc"

using namespace llvm;

MCS51Subtarget::MCS51Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const TargetMachine &TM)
    : MCS51GenSubtargetInfo(TT, CPU, CPU, FS), InstrInfo(*this), TLInfo(TM, *this) {
  ParseSubtargetFeatures(CPU, CPU, FS);
}
