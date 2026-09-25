// Fixed-width implicit-operand instructions use this compact record. Explicit
// operands and multi-byte encodings are described separately below.
def MCS51retglue : SDNode<"MCS51ISD::RET_GLUE", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def MCS51retword : SDNode<"MCS51ISD::RET_WORD", SDTNone, [SDNPHasChain]>;
def SDT_MCS51Call : SDTypeProfile<0, -1, [SDTCisVT<0, i16>]>;
def MCS51call : SDNode<"MCS51ISD::CALL", SDT_MCS51Call,
                       [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                        SDNPVariadic]>;
def SDT_MCS51Br : SDTypeProfile<0, 3, [SDTCisVT<0, i8>,
                                       SDTCisSameAs<0, 1>,
                                       SDTCisVT<2, OtherVT>]>;
def MCS51br_eq : SDNode<"MCS51ISD::BR_EQ", SDT_MCS51Br, [SDNPHasChain]>;
def MCS51br_ne : SDNode<"MCS51ISD::BR_NE", SDT_MCS51Br, [SDNPHasChain]>;
def MCS51br_ult : SDNode<"MCS51ISD::BR_ULT", SDT_MCS51Br, [SDNPHasChain]>;
def MCS51br_uge : SDNode<"MCS51ISD::BR_UGE", SDT_MCS51Br, [SDNPHasChain]>;
def MCS51br_slt : SDNode<"MCS51ISD::BR_SLT", SDT_MCS51Br, [SDNPHasChain]>;
def MCS51br_sge : SDNode<"MCS51ISD::BR_SGE", SDT_MCS51Br, [SDNPHasChain]>;
def SDT_MCS51LoadStack8 : SDTypeProfile<1, 1, [SDTCisVT<0, i8>,
                                               SDTCisVT<1, i8>]>;
def MCS51loadstack8 : SDNode<"MCS51ISD::LOAD_STACK8", SDT_MCS51LoadStack8,
                             [SDNPHasChain]>;
def SDT_MCS51LoadStack16 : SDTypeProfile<1, 1, [SDTCisVT<0, i16>,
                                                SDTCisVT<1, i8>]>;
def MCS51loadstack16 : SDNode<"MCS51ISD::LOAD_STACK16", SDT_MCS51LoadStack16,
                              [SDNPHasChain]>;
def SDT_MCS51PushArg8 : SDTypeProfile<0, 1, [SDTCisVT<0, i8>]>;
def MCS51pusharg8 : SDNode<"MCS51ISD::PUSH_ARG8", SDT_MCS51PushArg8,
                           [SDNPHasChain]>;
def SDT_MCS51PushArg16 : SDTypeProfile<0, 1, [SDTCisVT<0, i16>]>;
def MCS51pusharg16 : SDNode<"MCS51ISD::PUSH_ARG16", SDT_MCS51PushArg16,
                            [SDNPHasChain]>;
def MCS51poparg8 : SDNode<"MCS51ISD::POP_ARG8", SDTNone, [SDNPHasChain]>;
def MCS51pushpad8 : SDNode<"MCS51ISD::PUSH_PAD8", SDTNone, [SDNPHasChain]>;

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
def RRC_A : MCS51Inst<0x13, "rrc a"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def DEC_A : MCS51Inst<0x14, "dec a">;
def RL_A : MCS51Inst<0x23, "rl a">;
def RLC_A : MCS51Inst<0x33, "rlc a"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def MUL_AB : MCS51Inst<0xA4, "mul ab"> {
  let Defs = [A, B, C]; let Uses = [A, B];
  let hasSideEffects = 0;
}
let Defs = [A, B, C], Uses = [A, B] in
def DIV_AB : MCS51Inst<0x84, "div ab"> { let hasSideEffects = 0; }
def SWAP_A : MCS51Inst<0xC4, "swap a">;
def DA_A : MCS51Inst<0xD4, "da a">;
def CLR_A : MCS51Inst<0xE4, "clr a"> {
  let Defs = [A]; let hasSideEffects = 0;
}
def CPL_A : MCS51Inst<0xF4, "cpl a">;
def CLR_C : MCS51Inst<0xC3, "clr c"> { let Defs = [C]; }
def CPL_C : MCS51Inst<0xB3, "cpl c">;
def SETB_C : MCS51Inst<0xD3, "setb c">;
def JMP_ADPTR : MCS51Inst<0x73, "jmp @a+dptr">;
def MOVC_ADPTR : MCS51Inst<0x93, "movc a, @a+dptr"> {
  let Defs = [A]; let Uses = [A, DPTR];
}
def INC_DPTR : MCS51Inst<0xA3, "inc dptr"> {
  let Defs = [DPTR]; let Uses = [DPTR]; let hasSideEffects = 0;
}
def MOVC_APC : MCS51Inst<0x83, "movc a, @a+pc">;
def MOVX_ADPTR : MCS51Inst<0xE0, "movx a, @dptr"> {
  let Defs = [A]; let Uses = [DPTR];
}
class MCS51IndirectLoadInst<bits<8> opcode, string mnemonic>
    : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Indirect8:$ri);
  let AsmString = mnemonic # " @$ri";
  let Size = 1;
  let Defs = [A];
  let mayLoad = 1;
  bits<1> ri;
  bits<8> Inst;
  let Inst{7-1} = opcode{7-1};
  let Inst{0} = ri;
}
class MCS51IndirectStoreInst<bits<8> opcode, string mnemonic>
    : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Indirect8:$ri);
  let AsmString = mnemonic # " @$ri, a";
  let Size = 1;
  let Uses = [A];
  let mayStore = 1;
  bits<1> ri;
  bits<8> Inst;
  let Inst{7-1} = opcode{7-1};
  let Inst{0} = ri;
}
def MOV_A_IND_RI : MCS51IndirectLoadInst<0xE6, "mov a,">;
def MOV_IND_RI_A : MCS51IndirectStoreInst<0xF6, "mov">;
def MOVX_A_IND_RI : MCS51IndirectLoadInst<0xE2, "movx a,">;
def MOVX_IND_RI_A : MCS51IndirectStoreInst<0xF2, "movx">;
def MOVX_AR0 : MCS51Inst<0xE2, "movx a, @r0">;
def MOVX_AR1 : MCS51Inst<0xE3, "movx a, @r1">;
def MOVX_DPTRA : MCS51Inst<0xF0, "movx @dptr, a"> {
  let Uses = [A];
}
def RET16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let isReturn = 1;
  let isTerminator = 1;
}
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
def : Pat<(MCS51retword), (RET16)>;

def MCS51Imm8 : Operand<i8>,
                ImmLeaf<i8, "return isUInt<8>(Imm) || isInt<8>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm8OpValue";
  let DecoderMethod = "DecodeImm8";
}

def MCS51FrameIndex : Operand<i16>;

def MCS51Direct8 : Operand<i8>,
                   ImmLeaf<i8, "return isUInt<8>(Imm) || isInt<8>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm8OpValue";
  let DecoderMethod = "DecodeImm8";
}

def MCS51Addr16 : Operand<OtherVT> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm16OpValue";
  let DecoderMethod = "DecodeImm16";
}

def MCS51Imm16 : Operand<i16>, ImmLeaf<i16, "return isUInt<16>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm16OpValue";
  let DecoderMethod = "DecodeImm16";
}

def MCS51Rel8 : Operand<OtherVT> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getRel8OpValue";
  let DecoderMethod = "DecodeRel8";
  let PrintMethod = "printPCRelImm";
}

class MCS51RelBranch<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Rel8:$target);
  let AsmString = mnemonic # " $target";
  let Size = 2;
  let hasSideEffects = 1;
  let isBranch = 1;
  let isTerminator = 1;
  bits<8> target;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = target;
}

class MCS51BitRelBranch<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$bitaddr, MCS51Rel8:$target);
  let AsmString = mnemonic # " $bitaddr, $target";
  let Size = 3;
  let hasSideEffects = 1;
  let isBranch = 1;
  let isTerminator = 1;
  bits<8> bitaddr;
  bits<8> target;
  bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = bitaddr;
  let Inst{23-16} = target;
}

class MCS51BitInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$bitaddr);
  let AsmString = mnemonic;
  let Size = 2;
  let hasSideEffects = 1;
  bits<8> bitaddr;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = bitaddr;
}

def MOV_C_BIT : MCS51BitInst<0xA2, "mov c, $bitaddr"> { let Defs = [C]; }
def MOV_BIT_C : MCS51BitInst<0x92, "mov $bitaddr, c"> { let Uses = [C]; }
def CLR_BIT : MCS51BitInst<0xC2, "clr $bitaddr">;
def SETB_BIT : MCS51BitInst<0xD2, "setb $bitaddr">;
def CPL_BIT : MCS51BitInst<0xB2, "cpl $bitaddr">;
def ANL_C_BIT : MCS51BitInst<0x82, "anl c, $bitaddr"> { let Defs = [C]; }
def ANL_C_NOTBIT : MCS51BitInst<0xB0, "anl c, /$bitaddr"> { let Defs = [C]; }
def ORL_C_BIT : MCS51BitInst<0x72, "orl c, $bitaddr"> { let Defs = [C]; }
def ORL_C_NOTBIT : MCS51BitInst<0xA0, "orl c, /$bitaddr"> { let Defs = [C]; }

let isBarrier = 1 in
def SJMP : MCS51RelBranch<0x80, "sjmp">;
def JZ : MCS51RelBranch<0x60, "jz"> { let Uses = [A]; }
def JNZ : MCS51RelBranch<0x70, "jnz"> { let Uses = [A]; }
def JC : MCS51RelBranch<0x40, "jc"> { let Uses = [C]; }
def JNC : MCS51RelBranch<0x50, "jnc"> { let Uses = [C]; }
def JB : MCS51BitRelBranch<0x20, "jb">;
def JNB : MCS51BitRelBranch<0x30, "jnb">;
def JBC : MCS51BitRelBranch<0x10, "jbc">;

def BRCOND8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$cond, MCS51Addr16:$target);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let isBranch = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(brcond MCS51GPR8:$cond, bb:$target)];
}

class MCS51BrCCImmPseudo<SDNode Node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51Imm8:$rhs,
                           MCS51Addr16:$target);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let isBranch = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [A, R1, C];
  let Pattern = [(Node MCS51GPR8:$lhs, MCS51Imm8:$rhs, bb:$target)];
}

class MCS51BrCCRegPseudo<SDNode Node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs,
                           MCS51Addr16:$target);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let isBranch = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [A, R1, C];
  let Pattern = [(Node MCS51GPR8:$lhs, MCS51GPR8:$rhs, bb:$target)];
}

def BR_EQ8ri : MCS51BrCCImmPseudo<MCS51br_eq>;
def BR_EQ8rr : MCS51BrCCRegPseudo<MCS51br_eq>;
def BR_NE8ri : MCS51BrCCImmPseudo<MCS51br_ne>;
def BR_NE8rr : MCS51BrCCRegPseudo<MCS51br_ne>;
def BR_ULT8ri : MCS51BrCCImmPseudo<MCS51br_ult>;
def BR_ULT8rr : MCS51BrCCRegPseudo<MCS51br_ult>;
def BR_UGE8ri : MCS51BrCCImmPseudo<MCS51br_uge>;
def BR_UGE8rr : MCS51BrCCRegPseudo<MCS51br_uge>;
def BR_SLT8ri : MCS51BrCCImmPseudo<MCS51br_slt>;
def BR_SLT8rr : MCS51BrCCRegPseudo<MCS51br_slt>;
def BR_SGE8ri : MCS51BrCCImmPseudo<MCS51br_sge>;
def BR_SGE8rr : MCS51BrCCRegPseudo<MCS51br_sge>;

def LOADX8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51PTR:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADX16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51PTR:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADCODE8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51PTR:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADCODE16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51PTR:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADDIRECT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADDIRECT16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def STOREDIRECT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
}

def STOREDIRECT16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51PTR:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
}

def STOREX8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51PTR:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
}

class MCS51IndirectMemPseudo<bits<1> IsLoad> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = !if(IsLoad, (outs MCS51GPR8:$dst), (outs));
  let InOperandList = !if(IsLoad,
                          (ins MCS51Indirect8:$addr),
                          (ins MCS51Indirect8:$addr, MCS51GPR8:$src));
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = IsLoad;
  let mayStore = !not(IsLoad);
}

def LOADI8 : MCS51IndirectMemPseudo<1>;
def STOREI8 : MCS51IndirectMemPseudo<0>;
def LOADP8 : MCS51IndirectMemPseudo<1>;
def STOREP8 : MCS51IndirectMemPseudo<0>;

class MCS51IndirectWordPseudo<bits<1> IsLoad> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = !if(IsLoad, (outs MCS51PTR:$dst), (outs));
  let InOperandList = !if(IsLoad,
                          (ins MCS51Indirect8:$addr),
                          (ins MCS51Indirect8:$addr, MCS51PTR:$src));
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = IsLoad;
  let mayStore = !not(IsLoad);
}

def LOADI16 : MCS51IndirectWordPseudo<1>;
def STOREI16 : MCS51IndirectWordPseudo<0>;
def LOADP16 : MCS51IndirectWordPseudo<1>;
def STOREP16 : MCS51IndirectWordPseudo<0>;

class MCS51Abs16Inst<bits<8> opcode, string mnemonic, Operand Address>
    : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins Address:$addr);
  let AsmString = mnemonic # " $addr";
  let Size = 3;
  let hasSideEffects = 1;
  bits<16> addr;
  bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{23-8} = addr;
}

def LJMP : MCS51Abs16Inst<0x02, "ljmp", MCS51Addr16> {
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
}
let Defs = [A, B, C, DPTR, SP, R0, R1, R2, R3, R4, R5, R6, R7],
    Uses = [SP, DPTR, R7, R6, R5, R4], isCall = 1 in
def LCALL : MCS51Abs16Inst<0x12, "lcall", MCS51Imm16> {
  let Pattern = [(MCS51call (i16 tglobaladdr:$addr))];
}

def : Pat<(MCS51call (i16 texternalsym:$addr)),
          (LCALL texternalsym:$addr)>;

def MOV_DPTR_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51Imm16:$imm);
  let AsmString = "mov dptr, #$imm";
  let Size = 3;
  let hasSideEffects = 0;
  bits<1> dst;
  let dst = 0;
  bits<16> imm;
  bits<24> Inst;
  let Inst{7-0} = 0x90;
  let Inst{23-8} = imm;
  let Pattern = [(set MCS51PTR:$dst, MCS51Imm16:$imm)];
}

def LOADSTACKARG8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Imm8:$spadjust);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1];
  let Uses = [SP];
}

def LOADSTACKARG16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51Imm8:$spadjust);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, R0, DPTR];
  let Uses = [SP];
}

def PUSHARG8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [A];
}

def PUSHARG16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51PTR:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [A, SP];
  let Uses = [DPTR, SP];
}

def PUSHPAD8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [A, SP];
}

def SPILL_LOAD8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1];
  let Uses = [SP];
}

def SPILL_STORE8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [A, R1];
  let Uses = [SP];
}

def SPILL_LOAD16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1, DPTR];
}

def SPILL_STORE16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51PTR:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [A, R1];
  let Uses = [DPTR, SP];
}

def LOAD_FRAME8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1];
  let Uses = [SP];
}

def STORE_FRAME8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [A, R1];
  let Uses = [SP];
}

def LOAD_FRAME8_INDEX : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51PTR:$index);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1];
  let Uses = [DPTR, SP];
}

def STORE_FRAME8_INDEX : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51PTR:$index, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [A, R1];
  let Uses = [DPTR, SP];
}

def POPARG8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [B];
}

def INCDPTR16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51PTR:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let Pattern = [(set MCS51PTR:$dst, (add MCS51PTR:$src, (i16 1)))];
}

// The MCS-51 has no PC-relative unconditional branch with a long enough
// range for arbitrary basic blocks. Use LJMP until branch relaxation can
// select shorter forms safely.
def : Pat<(br bb:$target), (LJMP MCS51Addr16:$target)>;

// C promotes shift counts to int, which is i16 in the MCS-51 ABI. This
// immediate-only operand keeps supported constant counts visible to ISel.
def MCS51ShiftAmount : Operand<i16>, ImmLeaf<i16, "return isUInt<8>(Imm);">;

class MCS51AccImmInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51Imm8:$imm);
  let AsmString = mnemonic # " a, #$imm";
  let Size = 2;
  let hasSideEffects = 0;
  bits<1> dst;
  let dst = 0;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
}

def MOV_A_IMM : MCS51AccImmInst<0x74, "mov"> {
  let Pattern = [(set MCS51AReg:$dst, MCS51Imm8:$imm)];
}
def ADD_A_IMM : MCS51AccImmInst<0x24, "add"> {
  let Defs = [A]; let Uses = [A];
}
def SUBB_A_IMM : MCS51AccImmInst<0x94, "subb"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def ANL_A_IMM : MCS51AccImmInst<0x54, "anl"> {
  let Defs = [A]; let Uses = [A];
}
def ORL_A_IMM : MCS51AccImmInst<0x44, "orl"> {
  let Defs = [A]; let Uses = [A];
}
def XRL_A_IMM : MCS51AccImmInst<0x64, "xrl"> {
  let Defs = [A]; let Uses = [A];
}

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
  // Direct addresses 0x82 and 0x83 alias DPL and DPH. Conservatively model
  // direct stores as clobbering the 16-bit DPTR value.
  let Defs = [DPTR];
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
  let Defs = [DPTR];
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
def PUSH_DIRECT : MCS51DirectInst<0xC0, "push"> {
  let Defs = [SP]; let Uses = [SP];
}
def POP_DIRECT : MCS51DirectInst<0xD0, "pop"> {
  let Defs = [SP]; let Uses = [SP];
}
def INC_DIRECT : MCS51DirectInst<0x05, "inc">;
def DEC_DIRECT : MCS51DirectInst<0x15, "dec">;
def MOV_R0_IND_IMM : MCS51IndirectImmInst<0x76, "mov @r0,">;
def MOV_R1_IND_IMM : MCS51IndirectImmInst<0x77, "mov @r1,">;

class MCS51RnInst<bits<5> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$rn);
  let AsmString = mnemonic # " $rn";
  let Size = 1;
  let hasSideEffects = 0;
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
  let hasSideEffects = 0;
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
  let hasSideEffects = 0;
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
  let hasSideEffects = 0;
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
def MOV_B_RN  : MCS51RnInst<0b10001, "mov b,"> { let Defs = [B]; }
def ADD_A_RN  : MCS51RnInst<0b00101, "add a,"> { let Defs = [A]; let Uses = [A]; }
def ANL_A_RN  : MCS51RnInst<0b01011, "anl a,"> { let Defs = [A]; let Uses = [A]; }
def ORL_A_RN  : MCS51RnInst<0b01001, "orl a,"> { let Defs = [A]; let Uses = [A]; }
def XRL_A_RN  : MCS51RnInst<0b01101, "xrl a,"> { let Defs = [A]; let Uses = [A]; }
def ADDC_A_RN : MCS51RnInst<0b00111, "addc a,"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def SUBB_A_RN : MCS51RnInst<0b10011, "subb a,"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def MOV_RN_A  : MCS51RnDefInst<0b11111, "mov">;
def INC_RN : MCS51RnInst<0b00001, "inc"> { let hasSideEffects = 1; }
def MOV_RN_IMM : MCS51RnImmInst<0b01111, "mov"> {
  let Pattern = [(set MCS51GPR8:$rn, MCS51Imm8:$imm)];
}
def MOV_RN_RM : MCS51RegCopyInst;

class MCS51ALUPseudoRR<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51AReg:$dst,
                      (node MCS51GPR8:$lhs, MCS51GPR8:$rhs))];
}

class MCS51ALUPseudoRI<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51Imm8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51AReg:$dst,
                      (node MCS51GPR8:$lhs, MCS51Imm8:$rhs))];
}

def ADD8rr : MCS51ALUPseudoRR<add>;
def AND8rr : MCS51ALUPseudoRR<and>;
def OR8rr  : MCS51ALUPseudoRR<or>;
def XOR8rr : MCS51ALUPseudoRR<xor>;
def SUB8rr : MCS51ALUPseudoRR<sub>;
def ADD8ri : MCS51ALUPseudoRI<add>;
def AND8ri : MCS51ALUPseudoRI<and>;
def OR8ri  : MCS51ALUPseudoRI<or>;
def XOR8ri : MCS51ALUPseudoRI<xor>;
def SUB8ri : MCS51ALUPseudoRI<sub>;

def MUL8rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51AReg:$dst,
                      (mul MCS51GPR8:$lhs, MCS51GPR8:$rhs))];
}

class MCS51DivModPseudo<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51AReg:$dst,
                      (node MCS51GPR8:$lhs, MCS51GPR8:$rhs))];
}

def UDIV8rr : MCS51DivModPseudo<udiv>;
def UREM8rr : MCS51DivModPseudo<urem>;

class MCS51ShiftPseudo<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51ShiftAmount:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51AReg:$dst,
                      (node MCS51GPR8:$lhs, MCS51ShiftAmount:$amount))];
}

def SHL8ri : MCS51ShiftPseudo<shl>;
def SRL8ri : MCS51ShiftPseudo<srl>;

def ZEXT8TO16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51PTR:$dst, (zext MCS51GPR8:$src))];
}

def TRUNC16TO8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51PTR:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51AReg:$dst, (trunc MCS51PTR:$src))];
}
