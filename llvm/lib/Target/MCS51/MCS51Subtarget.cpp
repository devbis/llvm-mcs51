#include "MCS51Subtarget.h"
#include "llvm/IR/RuntimeLibcalls.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "MCS51GenSubtargetInfo.inc"

using namespace llvm;

MCS51Subtarget::MCS51Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const TargetMachine &TM)
    : MCS51GenSubtargetInfo(TT, CPU, CPU, FS), InstrInfo(*this), TLInfo(TM, *this) {
  ParseSubtargetFeatures(CPU, CPU, FS);
}

void MCS51Subtarget::initLibcallLoweringInfo(
    LibcallLoweringInfo &Info) const {
  const struct {
    RTLIB::Libcall Op;
    RTLIB::LibcallImpl Impl;
  } LibraryCalls[] = {
      {RTLIB::UDIV_I16, RTLIB::impl___udivhi3},
      {RTLIB::SDIV_I16, RTLIB::impl___divhi3},
      {RTLIB::UREM_I16, RTLIB::impl___umodhi3},
      {RTLIB::SREM_I16, RTLIB::impl___modhi3},
      {RTLIB::UDIV_I32, RTLIB::impl___udivsi3},
      {RTLIB::SDIV_I32, RTLIB::impl___divsi3},
      {RTLIB::UREM_I32, RTLIB::impl___umodsi3},
      {RTLIB::SREM_I32, RTLIB::impl___modsi3},
      {RTLIB::UDIV_I64, RTLIB::impl___udivdi3},
      {RTLIB::SDIV_I64, RTLIB::impl___divdi3},
      {RTLIB::UREM_I64, RTLIB::impl___umoddi3},
      {RTLIB::SREM_I64, RTLIB::impl___moddi3},
      {RTLIB::SHL_I64, RTLIB::impl___ashldi3},
      {RTLIB::SRL_I64, RTLIB::impl___lshrdi3},
      {RTLIB::SRA_I64, RTLIB::impl___ashrdi3},
      {RTLIB::FPTOSINT_F32_I32, RTLIB::impl___fixsfsi},
      {RTLIB::FPTOUINT_F32_I32, RTLIB::impl___fixunssfsi},
      {RTLIB::SINTTOFP_I32_F32, RTLIB::impl___floatsisf},
      {RTLIB::UINTTOFP_I32_F32, RTLIB::impl___floatunsisf},
      {RTLIB::FPTOSINT_F32_I64, RTLIB::impl___fixsfdi},
      {RTLIB::FPTOUINT_F32_I64, RTLIB::impl___fixunssfdi},
      {RTLIB::SINTTOFP_I64_F32, RTLIB::impl___floatdisf},
      {RTLIB::UINTTOFP_I64_F32, RTLIB::impl___floatundisf},
      {RTLIB::ADD_F32, RTLIB::impl___addsf3},
      {RTLIB::SUB_F32, RTLIB::impl___subsf3},
      {RTLIB::MUL_F32, RTLIB::impl___mulsf3},
      {RTLIB::DIV_F32, RTLIB::impl___divsf3},
      {RTLIB::UO_F32, RTLIB::impl___unordsf2},
      {RTLIB::FCMP3_PRED_OEQ_F32, RTLIB::impl___eqsf2},
      {RTLIB::FCMP3_PRED_UNE_F32, RTLIB::impl___nesf2},
      {RTLIB::FCMP3_PRED_OGE_F32, RTLIB::impl___gesf2},
      {RTLIB::FCMP3_PRED_OLT_F32, RTLIB::impl___ltsf2},
      {RTLIB::FCMP3_PRED_OLE_F32, RTLIB::impl___lesf2},
      {RTLIB::FCMP3_PRED_OGT_F32, RTLIB::impl___gtsf2},
      {RTLIB::OEQ_F32, RTLIB::impl___eqsf2},
      {RTLIB::UNE_F32, RTLIB::impl___nesf2},
      {RTLIB::OGE_F32, RTLIB::impl___gesf2},
      {RTLIB::OLT_F32, RTLIB::impl___ltsf2},
      {RTLIB::OLE_F32, RTLIB::impl___lesf2},
      {RTLIB::OGT_F32, RTLIB::impl___gtsf2},
  };
  for (const auto &LC : LibraryCalls)
    Info.setLibcallImpl(LC.Op, LC.Impl);
}
