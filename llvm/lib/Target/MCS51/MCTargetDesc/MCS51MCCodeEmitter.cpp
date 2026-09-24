#include "MCS51MCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

namespace {
class MCS51MCCodeEmitter final : public MCCodeEmitter {
  const MCInstrInfo &MII;

  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

public:
  explicit MCS51MCCodeEmitter(const MCInstrInfo &MII) : MII(MII) {}

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &Bytes,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override {
    uint64_t Encoding = getBinaryCodeForInstr(MI, Fixups, STI);
    unsigned Size = MII.get(MI.getOpcode()).getSize();
    for (unsigned I = 0; I < Size; ++I)
      Bytes.push_back(static_cast<char>((Encoding >> (I * 8)) & 0xff));
  }
};
} // namespace

#define GET_INSTRINFO_MC_CODE
#include "MCS51GenMCCodeEmitter.inc"

MCCodeEmitter *llvm::createMCS51MCCodeEmitter(const MCInstrInfo &MII,
                                               MCContext &) {
  return new MCS51MCCodeEmitter(MII);
}
