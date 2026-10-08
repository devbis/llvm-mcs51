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
  StringRef AddressByteModifier;
  switch (MI->getOpcode()) {
  case MCS51::MOV_IM_SYMLO_XDATA:
    AddressByteModifier = "xlo8";
    break;
  case MCS51::MOV_IM_SYMHI_XDATA:
    AddressByteModifier = "xhi8";
    break;
  case MCS51::MOV_IM_SYMLO_CODE:
    AddressByteModifier = "clo8";
    break;
  case MCS51::MOV_IM_SYMHI_CODE:
    AddressByteModifier = "chi8";
    break;
  default:
    break;
  }
  if (!AddressByteModifier.empty()) {
    OS << "mov ";
    printOperand(MI, 0, OS);
    OS << ", #" << AddressByteModifier << '(';
    printOperand(MI, 1, OS);
    OS << ')';
    printAnnotation(OS, Annot);
    return;
  }
  if (MI->getOpcode() == MCS51::MOV_DPTR_XDATA_IMM ||
      MI->getOpcode() == MCS51::MOV_DPTR_CODE_IMM) {
    OS << "mov dptr, #"
       << (MI->getOpcode() == MCS51::MOV_DPTR_XDATA_IMM ? "xdata16(" : "code16(");
    printOperand(MI, 1, OS);
    OS << ')';
    printAnnotation(OS, Annot);
    return;
  }
  if (!printAliasInstr(MI, Address, OS))
    printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot);
}
