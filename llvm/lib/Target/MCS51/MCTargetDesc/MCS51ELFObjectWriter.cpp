#include "MCS51MCTargetDesc.h"
#include "MCS51FixupKinds.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

namespace {
class MCS51ELFObjectWriter final : public MCELFObjectTargetWriter {
public:
  explicit MCS51ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_8051,
                                /*HasRelocationAddend=*/true) {}

  // The linker moves symbols, not section offsets, when it shrinks a
  // linker-relaxable section, so references into one must name the symbol.
  bool needsRelocateWithSymbol(const MCValue &V, unsigned) const override {
    const MCSymbol *Sym = V.getAddSym();
    return Sym && Sym->isInSection() && Sym->getSection().isLinkerRelaxable();
  }

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    if (mc::isRelocation(Fixup.getKind()))
      return Fixup.getKind();
    if (Target.getSubSym()) {
      reportError(Fixup.getLoc(),
                  "MCS-51 cannot relocate a symbol difference; label "
                  "differences across linker-relaxable code require "
                  "-mno-relax");
      return ELF::R_8051_NONE;
    }
    if (IsPCRel) {
      if (Fixup.getKind() == MCS51::fixup_11)
        return ELF::R_8051_11;
      if (Fixup.getKind() == MCS51::fixup_pcrel8)
        return ELF::R_8051_PCREL8;
      reportError(Fixup.getLoc(), "unsupported PC-relative MCS-51 relocation");
      return ELF::R_8051_NONE;
    }
    switch (Fixup.getKind()) {
    case MCS51::fixup_8:
    case FK_Data_1:
      return ELF::R_8051_8;
    case MCS51::fixup_16:
    case FK_Data_2:
      return ELF::R_8051_16;
    case MCS51::fixup_16_be:
      return ELF::R_8051_16_BE;
    case MCS51::fixup_dptr16:
      return ELF::R_8051_DPTR16;
    case MCS51::fixup_dptr16_xdata:
      return ELF::R_8051_DPTR16_XDATA;
    case MCS51::fixup_dptr16_code:
      return ELF::R_8051_DPTR16_CODE;
    case MCS51::fixup_lo8_xdata:
      return ELF::R_8051_LO8_XDATA;
    case MCS51::fixup_hi8_xdata:
      return ELF::R_8051_HI8_XDATA;
    case MCS51::fixup_lo8_code:
      return ELF::R_8051_LO8_CODE;
    case MCS51::fixup_hi8_code:
      return ELF::R_8051_HI8_CODE;
    case MCS51::fixup_lo8:
      return ELF::R_8051_LO8;
    case MCS51::fixup_hi8:
      return ELF::R_8051_HI8;
    default:
      reportError(Fixup.getLoc(), "unsupported MCS-51 relocation");
      return ELF::R_8051_NONE;
    }
  }
};
} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createMCS51ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<MCS51ELFObjectWriter>(OSABI);
}
