#include "TargetInfo/MCS51TargetInfo.h"
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
    Size = 1;
    return decodeInstruction(DecoderTable8, Inst, Bytes[0], Address, this,
                             STI);
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
