#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "MCS51.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
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
};
} // namespace

char MCS51AsmPrinter::ID = 0;

INITIALIZE_PASS(MCS51AsmPrinter, "mcs51-asm-printer", "MCS-51 Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMCS51AsmPrinter() {
  RegisterAsmPrinter<MCS51AsmPrinter> X(getTheMCS51Target());
}
