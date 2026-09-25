// Fixed-width implicit-operand instructions use this compact record. Explicit
// operands and multi-byte encodings are described separately below.
def MCS51retglue : SDNode<"MCS51ISD::RET_GLUE", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

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
def MOV_A_R0_IND : MCS51Inst<0xE6, "mov a, @r0">;
def MOV_A_R1_IND : MCS51Inst<0xE7, "mov a, @r1">;
def MOV_R0_IND_A : MCS51Inst<0xF6, "mov @r0, a">;
def MOV_R1_IND_A : MCS51Inst<0xF7, "mov @r1, a">;
def INC_R0_IND : MCS51Inst<0x06, "inc @r0">;
def INC_R1_IND : MCS51Inst<0x07, "inc @r1">;
def RET  : MCS51Inst<0x22, "ret"> {
  let isReturn = 1;
  let isTerminator = 1;
  let Uses = [A];
}
def RETI : MCS51Inst<0x32, "reti"> {
  let isReturn = 1;
  let isTerminator = 1;
}

def : Pat<(MCS51retglue), (RET)>;

def MCS51Imm8 : Operand<i8>, ImmLeaf<i8, "return isUInt<8>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm8OpValue";
  let DecoderMethod = "DecodeImm8";
}

def MCS51Direct8 : Operand<i8>, ImmLeaf<i8, "return isUInt<8>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm8OpValue";
  let DecoderMethod = "DecodeImm8";
}

class MCS51AccImmInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51Imm8:$imm);
  let AsmString = mnemonic # " a, #$imm";
  let Size = 2;
  let hasSideEffects = 1;
  bits<1> dst;
  let dst = 0;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
}

class MCS51AccImmALUInst<bits<8> opcode, string mnemonic,
                         SDNode node> : MCS51AccImmInst<opcode, mnemonic> {
  let Pattern = [(set MCS51AReg:$dst,
                      (node A, MCS51Imm8:$imm))];
}

def MOV_A_IMM : MCS51AccImmInst<0x74, "mov"> {
  let Pattern = [(set MCS51AReg:$dst, MCS51Imm8:$imm)];
}
def ADD_A_IMM : MCS51AccImmALUInst<0x24, "add", add>;
def ANL_A_IMM : MCS51AccImmALUInst<0x54, "anl", and>;
def ORL_A_IMM : MCS51AccImmALUInst<0x44, "orl", or>;
def XRL_A_IMM : MCS51AccImmALUInst<0x64, "xrl", xor>;

class MCS51AccDirectInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = mnemonic # " a, $addr";
  let Size = 2;
  let hasSideEffects = 1;
  bits<1> dst;
  let dst = 0;
  bits<8> addr;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
}

class MCS51DirectAccInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = mnemonic # " $addr, a";
  let Size = 2;
  let hasSideEffects = 1;
  let Uses = [A];
  bits<8> addr;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
}

class MCS51DirectImmInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51Imm8:$imm);
  let AsmString = mnemonic # " $addr, #$imm";
  let Size = 3;
  let hasSideEffects = 1;
  bits<8> addr;
  bits<8> imm;
  bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
  let Inst{23-16} = imm;
}

class MCS51DirectInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = mnemonic # " $addr";
  let Size = 2;
  let hasSideEffects = 1;
  bits<8> addr;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
}

class MCS51IndirectImmInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$imm);
  let AsmString = mnemonic # " #$imm";
  let Size = 2;
  let hasSideEffects = 1;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
}

def MOV_A_DIRECT : MCS51AccDirectInst<0xE5, "mov">;
def MOV_DIRECT_A : MCS51DirectAccInst<0xF5, "mov">;
def MOV_DIRECT_IMM : MCS51DirectImmInst<0x75, "mov">;
def INC_DIRECT : MCS51DirectInst<0x05, "inc">;
def MOV_R0_IND_IMM : MCS51IndirectImmInst<0x76, "mov @r0,">;
def MOV_R1_IND_IMM : MCS51IndirectImmInst<0x77, "mov @r1,">;

class MCS51RnInst<bits<5> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$rn);
  let AsmString = mnemonic # " $rn";
  let Size = 1;
  let hasSideEffects = 1;
  bits<3> rn;
  bits<8> Inst;
  let Inst{7-3} = opcode;
  let Inst{2-0} = rn;
}

class MCS51RnImmInst<bits<5> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$rn);
  let InOperandList = (ins MCS51Imm8:$imm);
  let AsmString = mnemonic # " $rn, #$imm";
  let Size = 2;
  let hasSideEffects = 1;
  bits<3> rn;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-3} = opcode;
  let Inst{2-0} = rn;
  let Inst{15-8} = imm;
}

class MCS51RnDefInst<bits<5> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$rn);
  let InOperandList = (ins);
  let AsmString = mnemonic # " $rn, a";
  let Size = 1;
  let hasSideEffects = 1;
  bits<3> rn;
  bits<8> Inst;
  let Inst{7-3} = opcode;
  let Inst{2-0} = rn;
  let Uses = [A];
}

class MCS51RegCopyInst : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "mov $dst, $src";
  let Size = 3;
  let hasSideEffects = 1;
  bits<3> dst;
  bits<3> src;
  bits<24> Inst;
  let Inst{7-0} = 0x85;
  let Inst{15-11} = 0;
  let Inst{10-8} = src;
  let Inst{23-19} = 0;
  let Inst{18-16} = dst;
}

def MOV_A_RN  : MCS51RnInst<0b11101, "mov a,"> { let Defs = [A]; }
def ADD_A_RN  : MCS51RnInst<0b00101, "add a,">;
def ANL_A_RN  : MCS51RnInst<0b01011, "anl a,">;
def ORL_A_RN  : MCS51RnInst<0b01001, "orl a,">;
def XRL_A_RN  : MCS51RnInst<0b01101, "xrl a,">;
def ADDC_A_RN : MCS51RnInst<0b00111, "addc a,">;
def SUBB_A_RN : MCS51RnInst<0b10001, "subb a,">;
def MOV_RN_A  : MCS51RnDefInst<0b11111, "mov">;
def INC_RN : MCS51RnInst<0b00001, "inc">;
def MOV_RN_IMM : MCS51RnImmInst<0b01111, "mov"> {
  let Pattern = [(set MCS51GPR8:$rn, MCS51Imm8:$imm)];
}
def MOV_RN_RM : MCS51RegCopyInst;
