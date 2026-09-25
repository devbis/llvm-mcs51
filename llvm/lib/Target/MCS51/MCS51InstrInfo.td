// Fixed-width implicit-operand instructions use this compact record. Explicit
// operands and multi-byte encodings are described separately below.
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
def RR_A : MCS51Inst<0x03, "rr a">;
def INC_A : MCS51Inst<0x04, "inc a">;
def RRC_A : MCS51Inst<0x13, "rrc a">;
def DEC_A : MCS51Inst<0x14, "dec a">;
def RL_A : MCS51Inst<0x23, "rl a">;
def RLC_A : MCS51Inst<0x33, "rlc a">;
def MUL_AB : MCS51Inst<0xA4, "mul ab">;
def DIV_AB : MCS51Inst<0x84, "div ab">;
def SWAP_A : MCS51Inst<0xC4, "swap a">;
def DA_A : MCS51Inst<0xD4, "da a">;
def CLR_A : MCS51Inst<0xE4, "clr a">;
def CPL_A : MCS51Inst<0xF4, "cpl a">;
def CLR_C : MCS51Inst<0xC3, "clr c">;
def CPL_C : MCS51Inst<0xB3, "cpl c">;
def SETB_C : MCS51Inst<0xD3, "setb c">;
def JMP_ADPTR : MCS51Inst<0x73, "jmp @a+dptr">;
def MOVC_ADPTR : MCS51Inst<0x93, "movc a, @a+dptr">;
def MOVC_APC : MCS51Inst<0x83, "movc a, @a+pc">;
def MOVX_ADPTR : MCS51Inst<0xE0, "movx a, @dptr">;
def MOVX_AR0 : MCS51Inst<0xE2, "movx a, @r0">;
def MOVX_AR1 : MCS51Inst<0xE3, "movx a, @r1">;
def MOVX_DPTRA : MCS51Inst<0xF0, "movx @dptr, a">;
def MOVX_R0A : MCS51Inst<0xF2, "movx @r0, a">;
def MOVX_R1A : MCS51Inst<0xF3, "movx @r1, a">;
def RET  : MCS51Inst<0x22, "ret"> {
  let isReturn = 1;
  let isTerminator = 1;
}
def RETI : MCS51Inst<0x32, "reti"> {
  let isReturn = 1;
  let isTerminator = 1;
}
