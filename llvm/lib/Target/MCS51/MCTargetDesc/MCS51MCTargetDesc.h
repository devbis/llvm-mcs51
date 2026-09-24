#ifndef LLVM_LIB_TARGET_MCS51_MCTARGETDESC_MCS51MCTARGETDESC_H
#define LLVM_LIB_TARGET_MCS51_MCTARGETDESC_MCS51MCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/TargetParser/Triple.h"
#include <memory>

namespace llvm {
class Target;
class Triple;
class MCAsmInfo;
class MCInstPrinter;
class MCInstrInfo;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCContext;
class MCAsmBackend;
class MCCodeEmitter;
class MCTargetOptions;
class MCStreamer;
class MCObjectWriter;
class MCObjectTargetWriter;

MCInstrInfo *createMCS51MCInstrInfo();
MCRegisterInfo *createMCS51MCRegisterInfo(const Triple &TT);
MCSubtargetInfo *createMCS51MCSubtargetInfo(const Triple &TT, StringRef CPU,
                                             StringRef FS);
MCInstPrinter *createMCS51MCInstPrinter(const Triple &T,
                                        unsigned SyntaxVariant,
                                        const MCAsmInfo &MAI,
                                        const MCInstrInfo &MII,
                                        const MCRegisterInfo &MRI);
MCCodeEmitter *createMCS51MCCodeEmitter(const MCInstrInfo &MII,
                                        MCContext &Ctx);
MCAsmBackend *createMCS51MCAsmBackend(const Target &T,
                                      const MCSubtargetInfo &STI,
                                      const MCRegisterInfo &MRI,
                                      const MCTargetOptions &Options);
std::unique_ptr<MCObjectTargetWriter>
createMCS51ELFObjectWriter(uint8_t OSABI);

} // namespace llvm

#define GET_REGINFO_ENUM
#include "MCS51GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "MCS51GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "MCS51GenSubtargetInfo.inc"

#endif
