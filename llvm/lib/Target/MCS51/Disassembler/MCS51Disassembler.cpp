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
    switch (Opcode) {
    case 0x24: // ADD A,#data
    case 0x34: // ADDC A,#data
    case 0x44: // ORL A,#data
    case 0x54: // ANL A,#data
    case 0x64: // XRL A,#data
    case 0x74: // MOV A,#data
    case 0x94: // SUBB A,#data
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
