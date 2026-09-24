#include "MCS51MCTargetDesc.h"
#include "MCS51MCAsmInfo.h"
#include "MCS51InstPrinter.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "MCS51GenInstrInfo.inc"
#define GET_SUBTARGETINFO_MC_DESC
#include "MCS51GenSubtargetInfo.inc"
#define GET_REGINFO_MC_DESC
#include "MCS51GenRegisterInfo.inc"

MCInstrInfo *llvm::createMCS51MCInstrInfo() {
  auto *Info = new MCInstrInfo();
  InitMCS51MCInstrInfo(Info);
  return Info;
}

MCRegisterInfo *llvm::createMCS51MCRegisterInfo(const Triple &) {
  auto *Info = new MCRegisterInfo();
  InitMCS51MCRegisterInfo(Info, MCS51::PC);
  return Info;
}

MCSubtargetInfo *llvm::createMCS51MCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU,
                                                  StringRef FS) {
  return createMCS51MCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

MCInstPrinter *llvm::createMCS51MCInstPrinter(
    const Triple &T, unsigned SyntaxVariant, const MCAsmInfo &MAI,
    const MCInstrInfo &MII, const MCRegisterInfo &MRI) {
  if (SyntaxVariant != 0)
    return nullptr;
  return new MCS51InstPrinter(MAI, MII, MRI);
}

static MCStreamer *createMCS51ELFStreamer(
    const Triple &, MCContext &Context, std::unique_ptr<MCAsmBackend> &&Backend,
    std::unique_ptr<MCObjectWriter> &&Writer,
    std::unique_ptr<MCCodeEmitter> &&Emitter) {
  return createELFStreamer(Context, std::move(Backend), std::move(Writer),
                           std::move(Emitter));
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMCS51TargetMC() {
  Target &T = getTheMCS51Target();
  RegisterMCAsmInfo<MCS51MCAsmInfo> X(T);
  TargetRegistry::RegisterMCInstrInfo(T, createMCS51MCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createMCS51MCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createMCS51MCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createMCS51MCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createMCS51MCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createMCS51MCAsmBackend);
  TargetRegistry::RegisterELFStreamer(T, createMCS51ELFStreamer);
}
