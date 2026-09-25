#include "MCS51InstrInfo.h"
#include "MCS51Subtarget.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "MCS51GenInstrInfo.inc"

using namespace llvm;

MCS51InstrInfo::MCS51InstrInfo(const MCS51Subtarget &STI)
    : MCS51GenInstrInfo(STI, RI, ~0u, ~0u, ~0u, MCS51::RET), RI() {}
