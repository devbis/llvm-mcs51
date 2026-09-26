#include "MCS51.h"
#include "MCS51Banking.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Pass.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class MCS51AsmPrinter final : public AsmPrinter {
public:
  MCS51AsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}
  static char ID;

  StringRef getPassName() const override { return "MCS-51 Assembly Printer"; }

  void emitFunctionBodyEnd() override {
    const Function &F = MF->getFunction();
    unsigned Bank = F.hasSection() ? getMCS51CodeBank(F.getSection()) : 0;
    if (!Bank)
      return;

    MCSection *SavedSection = OutStreamer->getCurrentSectionOnly();
    MCSection *ThunkSection = OutContext.getELFSection(
        ".text.bankthunks", ELF::SHT_PROGBITS,
        ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
    OutStreamer->switchSection(ThunkSection);

    MCSymbol *Thunk = OutContext.getOrCreateSymbol(
        getMCS51BankThunkName(getSymbol(&F)->getName()));
    OutStreamer->emitSymbolAttribute(Thunk, MCSA_Global);
    OutStreamer->emitLabel(Thunk);
    emitBankThunkInstruction(MCS51::PUSH_DIRECT, {0x9f});
    emitBankThunkInstruction(MCS51::MOV_DIRECT_IMM, {0x9f, Bank});
    MCInst Call;
    Call.setOpcode(MCS51::LCALL);
    Call.addOperand(MCOperand::createExpr(
        MCSymbolRefExpr::create(getSymbol(&F), OutContext)));
    EmitToStreamer(*OutStreamer, Call);
    emitBankThunkInstruction(MCS51::POP_DIRECT, {0x9f});
    emitBankThunkInstruction(MCS51::RET, {});
    OutStreamer->switchSection(SavedSection);
  }

  void emitInstruction(const MachineInstr *MI) override {
    MCInst Inst;
    Inst.setOpcode(MI->getOpcode());
    for (const MachineOperand &MO : MI->operands()) {
      if (MO.isReg())
        Inst.addOperand(MCOperand::createReg(MO.getReg()));
      else if (MO.isImm())
        Inst.addOperand(MCOperand::createImm(MO.getImm()));
      else if (MO.isMBB())
        Inst.addOperand(MCOperand::createExpr(
            MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext)));
      else if (MO.isGlobal()) {
        const MCExpr *Expr =
            MCSymbolRefExpr::create(getSymbol(MO.getGlobal()), OutContext);
        if (MO.getOffset())
          Expr = MCBinaryExpr::createAdd(
              Expr, MCConstantExpr::create(MO.getOffset(), OutContext),
              OutContext);
        Inst.addOperand(MCOperand::createExpr(Expr));
      } else if (MO.isSymbol()) {
        const MCExpr *Expr = MCSymbolRefExpr::create(
            GetExternalSymbolSymbol(MO.getSymbolName()), OutContext);
        if (MO.getOffset())
          Expr = MCBinaryExpr::createAdd(
              Expr, MCConstantExpr::create(MO.getOffset(), OutContext),
              OutContext);
        Inst.addOperand(MCOperand::createExpr(Expr));
      } else
        report_fatal_error("unsupported MCS-51 machine operand in AsmPrinter");
    }
    EmitToStreamer(*OutStreamer, Inst);
  }

private:
  void emitBankThunkInstruction(unsigned Opcode,
                               ArrayRef<int64_t> Immediates) {
    MCInst Inst;
    Inst.setOpcode(Opcode);
    for (int64_t Imm : Immediates)
      Inst.addOperand(MCOperand::createImm(Imm));
    EmitToStreamer(*OutStreamer, Inst);
  }
};
} // namespace

char MCS51AsmPrinter::ID = 0;

INITIALIZE_PASS(MCS51AsmPrinter, "mcs51-asm-printer", "MCS-51 Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMCS51AsmPrinter() {
  RegisterAsmPrinter<MCS51AsmPrinter> X(getTheMCS51Target());
}
