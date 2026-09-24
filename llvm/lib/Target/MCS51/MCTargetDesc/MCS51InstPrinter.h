#ifndef LLVM_LIB_TARGET_MCS51_MCTARGETDESC_MCS51INSTPRINTER_H
#define LLVM_LIB_TARGET_MCS51_MCTARGETDESC_MCS51INSTPRINTER_H

#include "llvm/MC/MCInstPrinter.h"

namespace llvm {
class MCS51InstPrinter : public MCInstPrinter {
public:
  using MCInstPrinter::MCInstPrinter;
  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &O) override;
  void printRegName(raw_ostream &O, MCRegister Reg) override;
  std::pair<const char *, uint64_t>
  getMnemonic(const MCInst &MI) const override;
  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &O);
  bool printAliasInstr(const MCInst *MI, uint64_t Address, raw_ostream &O);
  void printCustomAliasOperand(const MCInst *MI, uint64_t Address,
                               unsigned OpIdx, unsigned PrintMethodIdx,
                               raw_ostream &O);
  static const char *getRegisterName(MCRegister Reg);
};
} // namespace llvm

#endif
