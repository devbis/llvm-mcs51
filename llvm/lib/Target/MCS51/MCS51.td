include "llvm/Target/Target.td"
include "MCS51RegisterInfo.td"
include "MCS51InstrInfo.td"
include "MCS51CallingConv.td"

defm : RemapAllTargetPseudoPointerOperands<MCS51PTR>;

def MCS51InstrInfo : InstrInfo;

// Linker relaxation is opt-in: when enabled, MOV DPTR,#imm16 fixups are
// emitted with R_8051_RELAX markers and references into the section keep
// their symbols so the linker can shrink the code safely.
def FeatureRelax : SubtargetFeature<"relax", "EnableLinkerRelax", "true",
                                    "Enable linker relaxation">;

// CC2530 uses the classic MCS-51 instruction core. Keep it as an explicit
// CPU choice so drivers and future device-specific scheduling/feature data
// have a stable target name.
def : Processor<"generic", NoItineraries, []>;
def : Processor<"cc2530", NoItineraries, []>;

def MCS51AsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def MCS51AsmParser : AsmParser {
  let ShouldEmitMatchRegisterName = 1;
}

def MCS51AsmParserVariant : AsmParserVariant {
  int Variant = 0;
}

def MCS51 : Target {
  let InstructionSet = MCS51InstrInfo;
  let AssemblyWriters = [MCS51AsmWriter];
  let AssemblyParsers = [MCS51AsmParser];
  let AssemblyParserVariants = [MCS51AsmParserVariant];
}
