#include "TargetInfo/MCS51TargetInfo.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;
using namespace llvm::MCD;
using DecodeStatus = MCDisassembler::DecodeStatus;

static DecodeStatus DecodeImm8(MCInst &Inst, unsigned Imm, uint64_t,
                               const MCDisassembler *) {
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeImm16(MCInst &Inst, unsigned Imm, uint64_t,
                                const MCDisassembler *) {
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeRel8(MCInst &Inst, unsigned Imm, uint64_t Address,
                               const MCDisassembler *) {
  unsigned Size = Inst.getOpcode() == MCS51::JB ||
                          Inst.getOpcode() == MCS51::JNB ||
                          Inst.getOpcode() == MCS51::JBC
                      ? 3
                      : 2;
  Inst.addOperand(MCOperand::createImm(static_cast<int64_t>(
      Address + Size + static_cast<int8_t>(Imm))));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeMCS51GPR8RegisterClass(
    MCInst &Inst, unsigned RegNo, uint64_t, const MCDisassembler *) {
  static constexpr MCRegister Registers[] = {
      MCS51::R0, MCS51::R1, MCS51::R2, MCS51::R3,
      MCS51::R4, MCS51::R5, MCS51::R6, MCS51::R7};
  if (RegNo >= 8)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(Registers[RegNo]));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeMCS51Indirect8RegisterClass(
    MCInst &Inst, unsigned RegNo, uint64_t, const MCDisassembler *) {
  if (RegNo >= 2)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(RegNo == 0 ? MCS51::R0 : MCS51::R1));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeMCS51ARegRegisterClass(
    MCInst &Inst, unsigned RegNo, uint64_t, const MCDisassembler *) {
  if (RegNo != 0)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(MCS51::A));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeMCS51PTRRegisterClass(
    MCInst &Inst, unsigned RegNo, uint64_t, const MCDisassembler *) {
  if (RegNo != 0)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(MCS51::DPTR));
  return MCDisassembler::Success;
}

#define GET_DISASSEMBLER_TABLE
#include "MCS51GenDisassemblerTables.inc"

namespace {
class MCS51Disassembler final : public MCDisassembler {
public:
  MCS51Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Inst, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &) const override {
    if (Bytes.empty()) {
      Size = 0;
      return Fail;
    }
    uint8_t Opcode = Bytes[0];
    if (Opcode == 0x02 || Opcode == 0x12 || Opcode == 0x10 || Opcode == 0x20 ||
        Opcode == 0x30 || Opcode == 0x75 || Opcode == 0x85 ||
        Opcode == 0x90) {
      // Long branches, bit branches, and three-byte MOV instructions.
      if (Bytes.size() < 3) {
        Size = 0;
        return Fail;
      }
      Size = 3;
      uint32_t InstBits = uint32_t(Bytes[0]) | (uint32_t(Bytes[1]) << 8) |
                          (uint32_t(Bytes[2]) << 16);
      return decodeInstruction(DecoderTable24, Inst, InstBits, Address, this,
                               STI);
    }
    switch (Opcode) {
    case 0x05: // INC direct
    case 0x40: // JC rel
    case 0x50: // JNC rel
    case 0x60: // JZ rel
    case 0x70: // JNZ rel
    case 0x80: // SJMP rel
    case 0x82: // ANL C,bit
    case 0x92: // MOV bit,C
    case 0xA0: // ORL C,/bit
    case 0xA2: // MOV C,bit
    case 0xB0: // ANL C,/bit
    case 0xB2: // CPL bit
    case 0xC2: // CLR bit
    case 0xD2: // SETB bit
    case 0x72: // ORL C,bit
    case 0xE5: // MOV A,direct
    case 0xF5: // MOV direct,A
    case 0x24: // ADD A,#data
    case 0x34: // ADDC A,#data
    case 0x44: // ORL A,#data
    case 0x54: // ANL A,#data
    case 0x64: // XRL A,#data
    case 0x74: // MOV A,#data
    case 0x76: // MOV @R0,#data
    case 0x77: // MOV @R1,#data
    case 0x94: // SUBB A,#data
    case 0xC0: // PUSH direct
    case 0xD0: // POP direct
    case 0x78: // MOV R0-R7,#data
    case 0x79:
    case 0x7a:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
    case 0x7f:
      if (Bytes.size() < 2) {
        Size = 0;
        return Fail;
      }
      Size = 2;
      return decodeInstruction(DecoderTable16, Inst,
                               uint64_t(Opcode) | (uint64_t(Bytes[1]) << 8),
                               Address, this, STI);
    default:
      Size = 1;
      return decodeInstruction(DecoderTable8, Inst, Opcode, Address, this,
                               STI);
    }
  }
};
} // namespace

static MCDisassembler *createMCS51Disassembler(const Target &,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new MCS51Disassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMCS51Disassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheMCS51Target(),
                                         createMCS51Disassembler);
}
