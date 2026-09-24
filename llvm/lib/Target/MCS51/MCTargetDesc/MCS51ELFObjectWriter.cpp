#include "MCS51MCTargetDesc.h"
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
  unsigned getRelocType(const MCFixup &, const MCValue &,
                        bool) const override {
    llvm_unreachable("MCS-51 relocations are not implemented yet");
  }
};
} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createMCS51ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<MCS51ELFObjectWriter>(OSABI);
}
