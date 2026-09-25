#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Compiler.h"
#include <string>

using namespace llvm;

namespace {
class MCS51Operand final : public MCParsedAsmOperand {
  SMLoc Loc;
  std::string Token;
  MCRegister Reg;
  const MCExpr *Expr = nullptr;

public:
  MCS51Operand(SMLoc Loc, StringRef Token)
      : Loc(Loc), Token(Token), Reg() {}
  MCS51Operand(SMLoc Loc, MCRegister Reg) : Loc(Loc), Reg(Reg) {}
  MCS51Operand(SMLoc Loc, const MCExpr *Expr) : Loc(Loc), Expr(Expr) {}

  bool isToken() const override { return !Reg.isValid() && !Expr; }
  bool isReg() const override { return Reg.isValid(); }
  bool isImm() const override { return Expr != nullptr; }
  bool isMem() const override { return false; }
  MCRegister getReg() const override { return Reg; }
  const MCExpr *getImm() const { return Expr; }
  StringRef getToken() const { return Token; }
  SMLoc getStartLoc() const override { return Loc; }
  SMLoc getEndLoc() const override { return Loc; }
  void print(raw_ostream &OS, const MCAsmInfo &) const override { OS << Token; }
  void addRegOperands(MCInst &Inst, unsigned) const {
    Inst.addOperand(MCOperand::createReg(Reg));
  }
  void addImmOperands(MCInst &Inst, unsigned) const {
    if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }
};

class MCS51AsmParser final : public MCTargetAsmParser {
  MCAsmParser &Parser;

  MCRegister getRegister(StringRef Name) const {
    return StringSwitch<MCRegister>(Name.lower())
        .Case("a", MCS51::A)
        .Case("c", MCS51::C)
        .Case("b", MCS51::B)
        .Case("dptr", MCS51::DPTR)
        .Case("dpl", MCS51::DPL)
        .Case("dph", MCS51::DPH)
        .Case("sp", MCS51::SP)
        .Case("psw", MCS51::PSW)
        .Case("r0", MCS51::R0)
        .Case("r1", MCS51::R1)
        .Case("r2", MCS51::R2)
        .Case("r3", MCS51::R3)
        .Case("r4", MCS51::R4)
        .Case("r5", MCS51::R5)
        .Case("r6", MCS51::R6)
        .Case("r7", MCS51::R7)
        .Default(MCRegister());
  }

#define GET_ASSEMBLER_HEADER
#include "MCS51GenAsmMatcher.inc"

  bool parseRegister(MCRegister &Reg, SMLoc &Start, SMLoc &End) override {
    Start = Parser.getTok().getLoc();
    Reg = getRegister(Parser.getTok().getString());
    if (!Reg)
      return true;
    End = Parser.getTok().getEndLoc();
    Parser.Lex();
    return false;
  }
  ParseStatus tryParseRegister(MCRegister &, SMLoc &, SMLoc &) override {
    return ParseStatus::NoMatch;
  }

  bool parseInstruction(ParseInstructionInfo &, StringRef Name, SMLoc NameLoc,
                        OperandVector &Operands) override {
    Operands.push_back(std::make_unique<MCS51Operand>(NameLoc, Name));
    while (!Parser.getTok().is(AsmToken::EndOfStatement) &&
           !Parser.getTok().is(AsmToken::Eof)) {
      const AsmToken &Tok = Parser.getTok();
      if (Tok.is(AsmToken::Comma)) {
        Parser.Lex();
        continue;
      }
      if (Tok.is(AsmToken::At)) {
        SMLoc Loc = Tok.getLoc();
        std::string Addressing = "@";
        Parser.Lex();
        while (!Parser.getTok().is(AsmToken::Comma) &&
               !Parser.getTok().is(AsmToken::EndOfStatement) &&
               !Parser.getTok().is(AsmToken::Eof)) {
          Addressing += Parser.getTok().getString().str();
          Parser.Lex();
        }
        Operands.push_back(std::make_unique<MCS51Operand>(Loc, Addressing));
        continue;
      }
      if (Tok.is(AsmToken::Hash)) {
        SMLoc ImmLoc = Tok.getLoc();
        Operands.push_back(std::make_unique<MCS51Operand>(ImmLoc, "#"));
        Parser.Lex();
        const MCExpr *Expr = nullptr;
        if (Parser.parseExpression(Expr))
          return true;
        Operands.push_back(std::make_unique<MCS51Operand>(ImmLoc, Expr));
        continue;
      }
      MCRegister Reg = Tok.is(AsmToken::Identifier)
                           ? getRegister(Tok.getString())
                           : MCRegister();
      if (Reg)
        Operands.push_back(std::make_unique<MCS51Operand>(Tok.getLoc(), Reg));
      else
        Operands.push_back(
            std::make_unique<MCS51Operand>(Tok.getLoc(), Tok.getString()));
      Parser.Lex();
    }
    if (Parser.getTok().is(AsmToken::EndOfStatement))
      Parser.Lex();
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
