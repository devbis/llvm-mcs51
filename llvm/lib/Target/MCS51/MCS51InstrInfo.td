// The first instruction records establish the encoding conventions used by
// the MCS-51 MC layer. The complete instruction set is added in subsequent
// target work.
class MCS51Inst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = mnemonic;
  let Size = 1;
  let hasSideEffects = 1;
  bits<8> Inst;
  let Inst{7-0} = opcode;
}

def NOP  : MCS51Inst<0x00, "nop"> { let hasSideEffects = 0; }
def RET  : MCS51Inst<0x22, "ret"> {
  let isReturn = 1;
  let isTerminator = 1;
}
def RETI : MCS51Inst<0x32, "reti"> {
  let isReturn = 1;
  let isTerminator = 1;
}
