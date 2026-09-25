include "llvm/Target/Target.td"
include "MCS51RegisterInfo.td"
include "MCS51InstrInfo.td"
include "MCS51CallingConv.td"

defm : RemapAllTargetPseudoPointerOperands<MCS51PTR>;

def MCS51InstrInfo : InstrInfo;

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
