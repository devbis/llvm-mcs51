#include "MCS51MCTargetDesc.h"
#include "MCS51FixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"

using namespace llvm;

namespace {
class MCS51ELFObjectWriter final : public MCELFObjectTargetWriter {
public:
  explicit MCS51ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_8051,
                                /*HasRelocationAddend=*/true) {}

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &,
                        bool IsPCRel) const override {
    if (IsPCRel)
      llvm_unreachable("PC-relative MCS-51 relocations are not implemented");
    switch (Fixup.getKind()) {
    case MCS51::fixup_8:
    case FK_Data_1:
      return ELF::R_8051_8;
    default:
      llvm_unreachable("unsupported MCS-51 relocation");
    }
  }
};
} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createMCS51ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<MCS51ELFObjectWriter>(OSABI);
}
