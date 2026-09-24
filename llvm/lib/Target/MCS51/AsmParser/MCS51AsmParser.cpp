#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {
class MCS51Operand final : public MCParsedAsmOperand {
  SMLoc Loc;
  StringRef Token;

public:
  MCS51Operand(SMLoc Loc, StringRef Token) : Loc(Loc), Token(Token) {}

  bool isToken() const override { return true; }
  bool isReg() const override { return false; }
  bool isImm() const override { return false; }
  bool isMem() const override { return false; }
  MCRegister getReg() const override { llvm_unreachable("token has no register"); }
  StringRef getToken() const { return Token; }
  SMLoc getStartLoc() const override { return Loc; }
  SMLoc getEndLoc() const override { return Loc; }
  void print(raw_ostream &OS, const MCAsmInfo &) const override { OS << Token; }
  void addRegOperands(MCInst &, unsigned) const {}
  void addImmOperands(MCInst &, unsigned) const {}
};

class MCS51AsmParser final : public MCTargetAsmParser {
  MCAsmParser &Parser;

#define GET_ASSEMBLER_HEADER
#include "MCS51GenAsmMatcher.inc"

  bool parseRegister(MCRegister &, SMLoc &, SMLoc &) override { return true; }
  ParseStatus tryParseRegister(MCRegister &, SMLoc &, SMLoc &) override {
    return ParseStatus::NoMatch;
  }

  bool parseInstruction(ParseInstructionInfo &, StringRef Name, SMLoc NameLoc,
                        OperandVector &Operands) override {
    Operands.push_back(std::make_unique<MCS51Operand>(NameLoc, Name));
    return false;
  }

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &, OperandVector &Operands,
                               MCStreamer &Out, uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override {
    MCInst Inst;
    auto Result = MatchInstructionImpl(Operands, Inst, ErrorInfo,
                                       MatchingInlineAsm);
    if (Result != Match_Success)
      return Parser.Error(IDLoc, "invalid 8051 instruction");
    Out.emitInstruction(Inst, getSTI());
    return false;
  }

public:
  MCS51AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};
} // namespace

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "MCS51GenAsmMatcher.inc"

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMCS51AsmParser() {
  RegisterMCAsmParser<MCS51AsmParser> X(getTheMCS51Target());
}
