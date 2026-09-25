#include "MCS51MCTargetDesc.h"
#include "MCS51FixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/Endian.h"

using namespace llvm;

namespace {
class MCS51AsmBackend final : public MCAsmBackend {
public:
  MCS51AsmBackend() : MCAsmBackend(endianness::little) {}

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createMCS51ELFObjectWriter(ELF::ELFOSABI_STANDALONE);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    if (Kind == MCS51::fixup_8)
      return {"fixup_8", 0, 8, 0};
    return MCAsmBackend::getFixupKindInfo(Kind);
  }

  void applyFixup(const MCFragment &, const MCFixup &Fixup, const MCValue &,
                  uint8_t *Data, uint64_t Value, bool) override {
    if (Fixup.getKind() == MCS51::fixup_8)
      Data[Fixup.getOffset()] = static_cast<uint8_t>(Value);
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
