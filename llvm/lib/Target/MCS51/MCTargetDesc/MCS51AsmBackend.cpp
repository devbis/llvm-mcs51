#include "MCS51MCTargetDesc.h"
#include "MCS51FixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Endian.h"

using namespace llvm;

namespace {
class MCS51AsmBackend final : public MCAsmBackend {
public:
  MCS51AsmBackend() : MCAsmBackend(endianness::little) {}

  std::optional<bool> evaluateFixup(const MCFragment &, MCFixup &Fixup,
                                    MCValue &Target,
                                    uint64_t &Value) override {
    if (Fixup.getKind() != MCS51::fixup_11)
      return {};
    // A relocatable symbol's final 2 KiB page is unknown until link time.
    if (!Target.isAbsolute()) {
      Value = 0;
      return false;
    }
    return {};
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createMCS51ELFObjectWriter(ELF::ELFOSABI_STANDALONE);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    if (Kind == MCS51::fixup_8)
      return {"fixup_8", 0, 8, 0};
    if (Kind == MCS51::fixup_16)
      return {"fixup_16", 0, 16, 0};
    if (Kind == MCS51::fixup_16_be)
      return {"fixup_16_be", 0, 16, 0};
    if (Kind == MCS51::fixup_dptr16)
      return {"fixup_dptr16", 0, 16, 0};
    if (Kind == MCS51::fixup_pcrel8)
      return {"fixup_pcrel8", 0, 8, 0};
    if (Kind == MCS51::fixup_11)
      return {"fixup_11", 0, 11, 0};
    if (Kind == MCS51::fixup_lo8)
      return {"fixup_lo8", 0, 8, 0};
    if (Kind == MCS51::fixup_hi8)
      return {"fixup_hi8", 0, 8, 0};
    return MCAsmBackend::getFixupKindInfo(Kind);
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    if (Fixup.getKind() == MCS51::fixup_pcrel8 && IsResolved &&
        !isInt<8>(static_cast<int64_t>(Value)))
      getContext().reportError(Fixup.getLoc(),
                               "MCS-51 relative branch is out of range");
    if (Fixup.getKind() == MCS51::fixup_11) {
      if (!IsResolved) {
        Asm->getWriter().recordRelocation(F, Fixup, Target, Value);
        return;
      }
      Data[0] |= static_cast<uint8_t>((Value >> 3) & 0xe0);
      Data[1] |= static_cast<uint8_t>(Value);
      return;
    }
    if (!IsResolved)
      Asm->getWriter().recordRelocation(F, Fixup, Target, Value);
    if (mc::isRelocation(Fixup.getKind()))
      return;

    if (Fixup.getKind() == MCS51::fixup_hi8) {
      Data[0] |= static_cast<uint8_t>(Value >> 8);
      return;
    }
    if (Fixup.getKind() == MCS51::fixup_16_be ||
        Fixup.getKind() == MCS51::fixup_dptr16) {
      Data[0] |= static_cast<uint8_t>(Value >> 8);
      Data[1] |= static_cast<uint8_t>(Value);
      return;
    }

    MCFixupKindInfo Info = getFixupKindInfo(Fixup.getKind());
    unsigned NumBits = Info.TargetSize + Info.TargetOffset;
    unsigned NumBytes = (NumBits + 7) / 8;
    Value <<= Info.TargetOffset;
    for (unsigned I = 0; I < NumBytes; ++I)
      Data[I] |= static_cast<uint8_t>(Value >> (I * 8));
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *) const override {
    while (Count--)
      OS.write(static_cast<char>(0x00));
    return true;
  }
};
} // namespace

MCAsmBackend *llvm::createMCS51MCAsmBackend(const Target &,
                                            const MCSubtargetInfo &,
                                            const MCRegisterInfo &,
                                            const MCTargetOptions &) {
  return new MCS51AsmBackend();
}
