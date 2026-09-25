#include "MCS51MCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class MCS51MCCodeEmitter final : public MCCodeEmitter {
  const MCInstrInfo &MII;
  const MCRegisterInfo &MRI;

  uint32_t getMachineOpValue(const MCInst &, const MCOperand &Op,
                             SmallVectorImpl<MCFixup> &,
                             const MCSubtargetInfo &) const {
    if (Op.isReg())
      return MRI.getEncodingValue(Op.getReg());
    if (Op.isImm())
      return static_cast<uint32_t>(Op.getImm());
    report_fatal_error("unsupported MCS-51 machine operand encoding");
  }

  uint32_t getImm8OpValue(const MCInst &MI, unsigned OpNo,
                          SmallVectorImpl<MCFixup> &,
                          const MCSubtargetInfo &) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint8_t>(Op.getImm());
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value))
      return static_cast<uint8_t>(Value);
    report_fatal_error("symbolic MCS-51 immediates need relocations");
  }

  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

public:
  explicit MCS51MCCodeEmitter(const MCInstrInfo &MII,
                              const MCRegisterInfo &MRI)
      : MII(MII), MRI(MRI) {}

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
                                               MCContext &Ctx) {
  return new MCS51MCCodeEmitter(MII, *Ctx.getRegisterInfo());
}
