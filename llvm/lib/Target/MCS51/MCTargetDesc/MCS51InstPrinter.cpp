#include "MCS51InstPrinter.h"
#include "MCS51MCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/Twine.h"

using namespace llvm;

#define PRINT_ALIAS_INSTR
#include "MCS51GenAsmWriter.inc"

void MCS51InstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void MCS51InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    raw_ostream &OS) {
  if (OpNo >= MI->getNumOperands()) {
    report_fatal_error("invalid MCS-51 asm operand index");
  }
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    MCRegister Reg = Op.getReg();
    if (!Reg || Reg.id() >= MCS51::NUM_TARGET_REGS)
      report_fatal_error(Twine("invalid MCS-51 asm register ") +
                         Twine(Reg.id()) + " at opcode " +
                         Twine(MI->getOpcode()) + ", operand " +
                         Twine(OpNo));
    printRegName(OS, Op.getReg());
  } else if (Op.isImm())
    OS << Op.getImm();
  else if (Op.isExpr())
    MAI.printExpr(OS, *Op.getExpr());
  else
    llvm_unreachable("unsupported MCS-51 operand");
}

void MCS51InstPrinter::printPCRelImm(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &OS) {
  printOperand(MI, OpNo, OS);
}

void MCS51InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &,
                                 raw_ostream &OS) {
  if (!printAliasInstr(MI, Address, OS))
    printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot);
}
