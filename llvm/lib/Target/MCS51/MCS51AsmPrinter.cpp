#include "MCS51.h"
#include "MCS51Banking.h"
#include "MCTargetDesc/MCS51InstPrinter.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <initializer_list>

using namespace llvm;

namespace {
static bool isAutoBankFunction(const Function &F, StringRef CPU,
                               bool FunctionSections) {
  if (F.hasSection())
    return isMCS51AutoBankSection(F.getSection());
  return CPU.equals_insensitive("cc2530") && FunctionSections &&
         F.getName() != "main" && !F.hasFnAttribute("interrupt");
}

class MCS51AsmPrinter final : public AsmPrinter {
public:
  MCS51AsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}
  static char ID;

  StringRef getPassName() const override { return "MCS-51 Assembly Printer"; }

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override {
    if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
      return false;
    if (ExtraCode && ExtraCode[0])
      return true;

    const MachineOperand &MO = MI->getOperand(OpNo);
    if (MO.isReg()) {
      OS << MCS51InstPrinter::getRegisterName(MO.getReg());
      return false;
    }
    if (MO.isImm()) {
      OS << MO.getImm();
      return false;
    }
    if (MO.isGlobal()) {
      PrintSymbolOperand(MO, OS);
      return false;
    }
    if (MO.isMBB()) {
      OS << *MO.getMBB()->getSymbol();
      return false;
    }
    return true;
  }

  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode,
                             raw_ostream &OS) override {
    if (ExtraCode && ExtraCode[0])
      return true;
    const MachineOperand &MO = MI->getOperand(OpNo);
    if (!MO.isReg() ||
        (MO.getReg() != MCS51::R0 && MO.getReg() != MCS51::R1))
      return true;
    OS << '@' << MCS51InstPrinter::getRegisterName(MO.getReg());
    return false;
  }

  void emitFunctionEntryLabel() override {
    const Function &F = MF->getFunction();
    if (F.hasFnAttribute("interrupt") &&
        TM.getTargetCPU().equals_insensitive("cc2530") &&
        TM.Options.FunctionSections) {
      SmallString<64> SectionName(".mcs51.common.");
      SectionName.append(F.getName());
      MCSection *Section = OutContext.getELFSection(
          SectionName, ELF::SHT_PROGBITS,
          ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
      MF->setSection(Section);
      OutStreamer->switchSection(Section);
    }
    AsmPrinter::emitFunctionEntryLabel();
  }

  const MCExpr *lowerConstant(const Constant *CV, const Constant *BaseCV,
                              uint64_t Offset) override {
    if (const auto *F = dyn_cast<Function>(CV)) {
      unsigned Bank = F->hasSection() ? getMCS51CodeBank(F->getSection()) : 0;
      bool AutoBank = isAutoBankFunction(*F, TM.getTargetCPU(),
                                         TM.Options.FunctionSections);
      if (Bank || AutoBank) {
        const MCExpr *Address = MCSymbolRefExpr::create(
            getBankThunkSymbol(*F), OutContext);
        if (Offset)
          Address = MCBinaryExpr::createAdd(
              Address, MCConstantExpr::create(Offset, OutContext),
              OutContext);
        return Address;
      }
    }
    return AsmPrinter::lowerConstant(CV, BaseCV, Offset);
  }

  void emitFunctionBodyEnd() override {
    const Function &F = MF->getFunction();
    unsigned Bank = F.hasSection() ? getMCS51CodeBank(F.getSection()) : 0;
    bool AutoBank = isAutoBankFunction(F, TM.getTargetCPU(),
                                       TM.Options.FunctionSections);
    if (F.hasFnAttribute("interrupt")) {
      if (Bank || AutoBank)
        report_fatal_error(
            "MCS-51 interrupt handlers must reside in common flash");
      StringRef Vector = F.getFnAttribute("interrupt").getValueAsString();
      unsigned Number = 0;
      if (Vector.getAsInteger(10, Number) || Number > 17)
        report_fatal_error("invalid MCS-51 interrupt vector number");

      MCSection *SavedSection = OutStreamer->getCurrentSectionOnly();
      SmallString<32> SectionName;
      raw_svector_ostream(SectionName) << ".mcs51.vector." << Number;
      MCSection *VectorSection = OutContext.getELFSection(
          SectionName, ELF::SHT_PROGBITS,
          ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
      OutStreamer->switchSection(VectorSection);
      MCInst Jump;
      Jump.setOpcode(MCS51::LJMP);
      Jump.addOperand(MCOperand::createExpr(
          MCSymbolRefExpr::create(getSymbol(&F), OutContext)));
      EmitToStreamer(*OutStreamer, Jump);
      OutStreamer->switchSection(SavedSection);
    }

    bool HasIndirectCall = false;
    for (const MachineBasicBlock &MBB : *MF)
      for (const MachineInstr &MI : MBB)
        HasIndirectCall |= MI.getOpcode() == MCS51::ICALL;
    if (HasIndirectCall) {
      OutStreamer->emitLabel(getIndirectCallThunkSymbol(F));
      emitIndirectCallThunk();
    }

    if (TM.getTargetCPU().equals_insensitive("cc2530") &&
        TM.Options.FunctionSections)
      emitExternalBankCallThunks();

    if (!Bank && !AutoBank)
      return;

    MCSection *SavedSection = OutStreamer->getCurrentSectionOnly();
    SmallString<48> ThunkSectionName;
    raw_svector_ostream(ThunkSectionName)
        << (AutoBank ? ".text.autobankthunks." : ".text.bankthunks.")
        << MF->getFunctionNumber();
    MCSection *ThunkSection =
        OutContext.getELFSection(ThunkSectionName, ELF::SHT_PROGBITS,
                                 ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
    OutStreamer->switchSection(ThunkSection);

    MCSymbol *Thunk = OutContext.getOrCreateSymbol(
        getMCS51BankThunkName(getSymbol(&F)->getName()));
    OutStreamer->emitSymbolAttribute(
        Thunk, F.isWeakForLinker() ? MCSA_Weak : MCSA_Global);
    OutStreamer->emitLabel(Thunk);
    emitBankThunkInstruction(MCS51::PUSH_DIRECT, {0x9f});
    if (AutoBank) {
      MCInst SelectBank;
      SelectBank.setOpcode(MCS51::MOV_DIRECT_IMM);
      SelectBank.addOperand(MCOperand::createImm(0x9f));
      SelectBank.addOperand(MCOperand::createExpr(
          MCSymbolRefExpr::create(getSymbol(&F), OutContext)));
      EmitToStreamer(*OutStreamer, SelectBank);
    } else {
      emitBankThunkInstruction(MCS51::MOV_DIRECT_IMM, {0x9f, Bank});
    }
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
    if (MI->getOpcode() == MCS51::SUB16DEC) {
      auto Emit = [&](unsigned Opcode, ArrayRef<MCOperand> Operands) {
        MCInst Inst;
        Inst.setOpcode(Opcode);
        for (const MCOperand &Operand : Operands)
          Inst.addOperand(Operand);
        EmitToStreamer(*OutStreamer, Inst);
      };
      MCSymbol *SkipHighDecrement =
          OutContext.createTempSymbol("mcs51_sub16_skip_high", true);
      Emit(MCS51::MOV_A_DIRECT,
           {MCOperand::createReg(MCS51::A), MCOperand::createImm(0x82)});
      Emit(MCS51::JNZ,
           {MCOperand::createExpr(MCSymbolRefExpr::create(
               SkipHighDecrement, OutContext))});
      Emit(MCS51::DEC_DIRECT, {MCOperand::createImm(0x83)});
      OutStreamer->emitLabel(SkipHighDecrement);
      Emit(MCS51::DEC_DIRECT, {MCOperand::createImm(0x82)});
      return;
    }

    if (MI->getOpcode() == MCS51::ICALL) {
      emitBankThunkInstruction(MCS51::PUSH_DIRECT, {0x82});
      emitBankThunkInstruction(MCS51::PUSH_DIRECT, {0x83});
      MCInst Call;
      Call.setOpcode(MCS51::LCALL);
      Call.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(
          getIndirectCallThunkSymbol(MF->getFunction()), OutContext)));
      EmitToStreamer(*OutStreamer, Call);
      return;
    }

    if (MI->getOpcode() == MCS51::LCALL &&
        TM.getTargetCPU().equals_insensitive("cc2530") &&
        TM.Options.FunctionSections) {
      for (const MachineOperand &MO : MI->operands())
        if (MO.isSymbol()) {
          StringRef TargetName = MO.getSymbolName();
          if (TargetName.starts_with("__mcs51_bankcall_")) {
            MCInst Call;
            Call.setOpcode(MCS51::LCALL);
            Call.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(
                GetExternalSymbolSymbol(TargetName), OutContext)));
            EmitToStreamer(*OutStreamer, Call);
            return;
          }
          const Function *Target = MF->getFunction().getParent()->getFunction(
              TargetName);
          if (Target && !Target->isDeclaration() &&
              (getMCS51CodeBank(Target->getSection()) ||
               isAutoBankFunction(*Target, TM.getTargetCPU(),
                                  TM.Options.FunctionSections))) {
            MCInst Call;
            Call.setOpcode(MCS51::LCALL);
            Call.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(
                getBankThunkSymbol(*Target), OutContext)));
            EmitToStreamer(*OutStreamer, Call);
            return;
          }
          MCInst Call;
          Call.setOpcode(MCS51::LCALL);
          Call.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(
              getExternalBankCallThunkSymbol(TargetName),
              OutContext)));
          EmitToStreamer(*OutStreamer, Call);
          return;
        }
    }

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
        const GlobalValue *GV = MO.getGlobal();
        MCSymbol *Symbol = getSymbol(GV);
        if (const auto *Target = dyn_cast<Function>(GV)) {
          unsigned TargetBank = Target->hasSection()
                                    ? getMCS51CodeBank(Target->getSection())
                                    : 0;
          bool TargetAutoBank =
              isAutoBankFunction(*Target, TM.getTargetCPU(),
                                 TM.Options.FunctionSections);
          unsigned CallerBank = MF->getFunction().hasSection()
                                    ? getMCS51CodeBank(
                                          MF->getFunction().getSection())
                                    : 0;
          // A banked function's 16-bit code address is shared by every bank.
          // Use its common-area trampoline whenever the address escapes as a
          // value. Direct calls within the same bank still target the body.
          if ((TargetBank &&
               (MI->getOpcode() != MCS51::LCALL || TargetBank != CallerBank)) ||
              TargetAutoBank)
            Symbol = getBankThunkSymbol(*Target);
        }
        const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, OutContext);
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
  MCSymbol *getBankThunkSymbol(const Function &F) {
    return OutContext.getOrCreateSymbol(
        getMCS51BankThunkName(getSymbol(&F)->getName()));
  }

  MCSymbol *getIndirectCallThunkSymbol(const Function &F) {
    SmallString<64> Name(".L");
    Name.append(getSymbol(&F)->getName());
    Name.append(".mcs51.icall");
    return OutContext.getOrCreateSymbol(Name);
  }

  MCSymbol *getExternalBankCallThunkSymbol(StringRef TargetName) {
    return OutContext.getOrCreateSymbol(getMCS51BankThunkName(TargetName));
  }

  void emitExternalBankCallThunks() {
    SmallVector<StringRef, 4> Targets;
    for (const MachineBasicBlock &MBB : *MF)
      for (const MachineInstr &MI : MBB) {
        if (MI.getOpcode() != MCS51::LCALL)
          continue;
        for (const MachineOperand &MO : MI.operands()) {
          StringRef TargetName;
          const Function *Target = nullptr;
          if (MO.isSymbol()) {
            TargetName = MO.getSymbolName();
            if (TargetName.starts_with("__mcs51_bankcall_"))
              continue;
            Target =
                MF->getFunction().getParent()->getFunction(TargetName);
          } else if (MO.isGlobal()) {
            const GlobalValue *GV = MO.getGlobal();
            TargetName = GV->getName();
            Target = dyn_cast<Function>(GV);
          } else {
            continue;
          }
          // Definitions emit their own strong trampoline (when banked) or
          // can be called directly. Only declarations and unresolved symbols
          // need a weak caller-side fallback.
          if (Target && !Target->isDeclaration())
            continue;
          if (!ExternalBankCallThunks.contains(TargetName) &&
              !is_contained(Targets, TargetName))
            Targets.push_back(TargetName);
        }
      }
    if (Targets.empty())
      return;

    MCSection *SavedSection = OutStreamer->getCurrentSectionOnly();
    SmallString<48> ThunkSectionName;
    raw_svector_ostream(ThunkSectionName)
        << ".text.autobankthunks.external." << MF->getFunctionNumber();
    MCSection *ThunkSection = OutContext.getELFSection(
        ThunkSectionName, ELF::SHT_PROGBITS,
        ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
    OutStreamer->switchSection(ThunkSection);
    for (StringRef TargetName : Targets) {
      ExternalBankCallThunks.insert(TargetName);
      MCSymbol *Thunk = getExternalBankCallThunkSymbol(TargetName);
      OutStreamer->emitSymbolAttribute(Thunk, MCSA_Weak);
      OutStreamer->emitLabel(
          Thunk);
      emitBankThunkInstruction(MCS51::PUSH_DIRECT, {0x9f});
      MCInst SelectBank;
      SelectBank.setOpcode(MCS51::MOV_DIRECT_IMM);
      SelectBank.addOperand(MCOperand::createImm(0x9f));
      SelectBank.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(
          GetExternalSymbolSymbol(TargetName), OutContext)));
      EmitToStreamer(*OutStreamer, SelectBank);
      MCInst Call;
      Call.setOpcode(MCS51::LCALL);
      Call.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(
          GetExternalSymbolSymbol(TargetName), OutContext)));
      EmitToStreamer(*OutStreamer, Call);
      emitBankThunkInstruction(MCS51::POP_DIRECT, {0x9f});
      emitBankThunkInstruction(MCS51::RET, {});
    }
    OutStreamer->switchSection(SavedSection);
  }

  StringSet<> ExternalBankCallThunks;

  void emitThunkInstruction(unsigned Opcode,
                            std::initializer_list<MCOperand> Operands) {
    MCInst Inst;
    Inst.setOpcode(Opcode);
    for (const MCOperand &Operand : Operands)
      Inst.addOperand(Operand);
    EmitToStreamer(*OutStreamer, Inst);
  }

  void emitIndirectCallThunk() {
    auto Reg = [](unsigned R) { return MCOperand::createReg(R); };
    auto Imm = [](int64_t V) { return MCOperand::createImm(V); };

    // The caller pushed DPL and DPH before LCALL. The helper's stack is:
    // return-high, return-low, target-high, target-low, then stack arguments.
    // Load the target, move the helper return address over the target bytes,
    // and shrink SP so the indirect callee sees an ordinary call frame.
    emitThunkInstruction(MCS51::MOV_A_DIRECT, {Reg(MCS51::A), Imm(0x81)});
    emitThunkInstruction(MCS51::ADD_A_IMM, {Reg(MCS51::A), Imm(0xFE)});
    emitThunkInstruction(MCS51::MOV_RN_A, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_IND_RI, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_DIRECT_A, {Imm(0x83)});
    emitThunkInstruction(MCS51::DEC_RN,
                         {Reg(MCS51::R0), Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_IND_RI, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_DIRECT_A, {Imm(0x82)});

    emitThunkInstruction(MCS51::MOV_A_DIRECT, {Reg(MCS51::A), Imm(0x81)});
    emitThunkInstruction(MCS51::MOV_RN_A, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_IND_RI, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_RN_A, {Reg(MCS51::R2)});
    emitThunkInstruction(MCS51::DEC_RN,
                         {Reg(MCS51::R0), Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_IND_RI, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_RN_A, {Reg(MCS51::R3)});
    emitThunkInstruction(MCS51::DEC_RN,
                         {Reg(MCS51::R0), Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_RN, {Reg(MCS51::R2)});
    emitThunkInstruction(MCS51::MOV_IND_RI_A, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::DEC_RN,
                         {Reg(MCS51::R0), Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_RN, {Reg(MCS51::R3)});
    emitThunkInstruction(MCS51::MOV_IND_RI_A, {Reg(MCS51::R0)});
    emitThunkInstruction(MCS51::MOV_A_DIRECT, {Reg(MCS51::A), Imm(0x81)});
    emitThunkInstruction(MCS51::ADD_A_IMM, {Reg(MCS51::A), Imm(0xFE)});
    emitThunkInstruction(MCS51::MOV_DIRECT_A, {Imm(0x81)});
    emitThunkInstruction(MCS51::CLR_A, {});
    emitThunkInstruction(MCS51::JMP_ADPTR, {});
  }

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
