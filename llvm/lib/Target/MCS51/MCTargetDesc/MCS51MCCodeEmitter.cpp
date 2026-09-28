#include "MCS51MCTargetDesc.h"
#include "MCS51FixupKinds.h"
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
  MCContext &Ctx;

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
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint8_t>(Op.getImm());
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value))
      return static_cast<uint8_t>(Value);
    if (Op.isExpr()) {
      // This operand method is shared by byte fields at different positions.
      // MCS51DirectImmInst has its immediate in byte 2; every other use is
      // the second byte of a two-byte instruction.
      unsigned Offset = MI.getOpcode() == MCS51::MOV_DIRECT_IMM && OpNo == 1
                            ? 2
                            : 1;
      Fixups.push_back(
          MCFixup::create(Offset, Op.getExpr(), MCS51::fixup_8));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 immediate operand");
  }

  uint32_t getImm16OpValue(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint16_t>(Op.getImm());
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value))
      return static_cast<uint16_t>(Value);
    if (Op.isExpr()) {
      Fixups.push_back(
          MCFixup::create(1, Op.getExpr(), MCS51::fixup_16));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 address operand");
  }

  uint32_t getImm16BEOpValue(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm()) {
      uint16_t Value = static_cast<uint16_t>(Op.getImm());
      return static_cast<uint16_t>((Value << 8) | (Value >> 8));
    }
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value)) {
      uint16_t Absolute = static_cast<uint16_t>(Value);
      return static_cast<uint16_t>((Absolute << 8) | (Absolute >> 8));
    }
    if (Op.isExpr()) {
      unsigned Kind = MI.getOpcode() == MCS51::MOV_DPTR_IMM
                          ? MCS51::fixup_dptr16
                          : MCS51::fixup_16_be;
      Fixups.push_back(
          MCFixup::create(1, Op.getExpr(), Kind));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 big-endian address operand");
  }

  uint32_t getAddr11OpValue(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint16_t>(Op.getImm()) & 0x7ff;
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value))
      return static_cast<uint16_t>(Value) & 0x7ff;
    if (Op.isExpr()) {
      Fixups.push_back(MCFixup::create(0, Op.getExpr(), MCS51::fixup_11,
                                       true));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 11-bit address operand");
  }

  uint32_t getRel8OpValue(const MCInst &MI, unsigned OpNo,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint8_t>(Op.getImm());
    if (Op.isExpr()) {
      unsigned Offset = MI.getOpcode() == MCS51::DJNZ_RN || OpNo == 0 ? 1 : 2;
      const MCExpr *Expr = MCBinaryExpr::createSub(
          Op.getExpr(), MCConstantExpr::create(1, Ctx), Ctx);
      Fixups.push_back(
          MCFixup::create(Offset, Expr, MCS51::fixup_pcrel8, true));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 relative branch operand");
  }

  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

public:
  explicit MCS51MCCodeEmitter(const MCInstrInfo &MII,
                              const MCRegisterInfo &MRI, MCContext &Ctx)
      : MII(MII), MRI(MRI), Ctx(Ctx) {}

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
  return new MCS51MCCodeEmitter(MII, *Ctx.getRegisterInfo(), Ctx);
}
