#include "MCS51MCTargetDesc.h"
#include "MCS51FixupKinds.h"
#include "llvm/ADT/bit.h"
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

  // Byte index of operand OpNo within the instruction encoding, derived from
  // the TableGen field layout rather than a per-opcode list.
  unsigned getOperandByteOffset(const MCInst &MI, unsigned OpNo,
                                const MCSubtargetInfo &STI) const {
    MCInst Probe = MI;
    SmallVector<MCFixup, 4> Scratch;
    Probe.getOperand(OpNo) = MCOperand::createImm(0);
    uint64_t Zero = getBinaryCodeForInstr(Probe, Scratch, STI);
    Probe.getOperand(OpNo) = MCOperand::createImm(0xff);
    uint64_t Ones = getBinaryCodeForInstr(Probe, Scratch, STI);
    uint64_t Diff = Zero ^ Ones;
    unsigned Shift = Diff ? llvm::countr_zero(Diff) : 64;
    if (Shift % 8 != 0 || Diff != (uint64_t(0xff) << Shift))
      report_fatal_error("MCS-51 8-bit operand is not byte aligned");
    return Shift / 8;
  }

  uint32_t getImm8OpValue(const MCInst &MI, unsigned OpNo,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint8_t>(Op.getImm());
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value))
      return static_cast<uint8_t>(Value);
    if (Op.isExpr()) {
      // Shared by byte fields at different positions, e.g. the destination
      // of MOV direct,direct is byte 2 while its source is byte 1.
      unsigned Offset = getOperandByteOffset(MI, OpNo, STI);
      Fixups.push_back(
          MCFixup::create(Offset, Op.getExpr(), MCS51::fixup_8));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 immediate operand");
  }

  // The byte of a symbol address held in the third byte of
  // MOV direct,#imm style instructions.
  uint32_t getSymByteOpValue(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups, bool High) const {
    const MCOperand &Op = MI.getOperand(OpNo);
    if (Op.isImm())
      return static_cast<uint8_t>(High ? Op.getImm() >> 8 : Op.getImm());
    int64_t Value = 0;
    if (Op.isExpr() && Op.getExpr()->evaluateAsAbsolute(Value))
      return static_cast<uint8_t>(High ? Value >> 8 : Value);
    if (Op.isExpr()) {
      bool IsCode = MI.getOpcode() == MCS51::MOV_IM_SYMLO_CODE ||
                    MI.getOpcode() == MCS51::MOV_IM_SYMHI_CODE;
      bool IsXData = MI.getOpcode() == MCS51::MOV_IM_SYMLO_XDATA ||
                     MI.getOpcode() == MCS51::MOV_IM_SYMHI_XDATA;
      MCFixupKind Kind = High ? MCS51::fixup_hi8 : MCS51::fixup_lo8;
      if (IsCode)
        Kind = High ? MCS51::fixup_hi8_code : MCS51::fixup_lo8_code;
      else if (IsXData)
        Kind = High ? MCS51::fixup_hi8_xdata : MCS51::fixup_lo8_xdata;
      Fixups.push_back(MCFixup::create(
          2, Op.getExpr(), Kind));
      return 0;
    }
    report_fatal_error("unsupported MCS-51 symbol byte operand");
  }
  uint32_t getSymLo8OpValue(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &) const {
    return getSymByteOpValue(MI, OpNo, Fixups, false);
  }
  uint32_t getSymHi8OpValue(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &) const {
    return getSymByteOpValue(MI, OpNo, Fixups, true);
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
                             const MCSubtargetInfo &STI) const {
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
      bool IsXDataDPTRLoad = MI.getOpcode() == MCS51::MOV_DPTR_XDATA_IMM;
      bool IsCodeDPTRLoad = MI.getOpcode() == MCS51::MOV_DPTR_CODE_IMM;
      bool IsDPTRLoad = MI.getOpcode() == MCS51::MOV_DPTR_IMM ||
                        IsXDataDPTRLoad || IsCodeDPTRLoad;
      MCFixup Fixup = MCFixup::create(
          1, Op.getExpr(),
          IsXDataDPTRLoad
              ? MCS51::fixup_dptr16_xdata
              : IsCodeDPTRLoad
                    ? MCS51::fixup_dptr16_code
                    : IsDPTRLoad ? MCS51::fixup_dptr16 : MCS51::fixup_16_be);
      // With +relax the linker may shrink a MOV DPTR,#sym+1 that follows
      // MOV DPTR,#sym to INC DPTR.
      // LLD may also shrink LCALL/LJMP to ACALL/AJMP. Keep their fragments
      // linker-relaxable so local PC-relative branches spanning them remain
      // relocations and can be adjusted after the instruction is shortened.
      bool IsCodeTransfer = MI.getOpcode() == MCS51::LCALL ||
                            MI.getOpcode() == MCS51::LJMP;
      if ((IsDPTRLoad && STI.hasFeature(MCS51::FeatureRelax)) ||
          IsCodeTransfer)
        Fixup.setLinkerRelaxable();
      Fixups.push_back(Fixup);
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
