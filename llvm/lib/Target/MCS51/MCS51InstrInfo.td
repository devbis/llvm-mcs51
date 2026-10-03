// Fixed-width implicit-operand instructions use this compact record. Explicit
// operands and multi-byte encodings are described separately below.
def MCS51retglue : SDNode<"MCS51ISD::RET_GLUE", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def SDT_MCS51RetA : SDTypeProfile<0, 1, [SDTCisVT<0, i8>] >;
def MCS51ret_a : SDNode<"MCS51ISD::RET_A", SDT_MCS51RetA, [SDNPHasChain]>;
def MCS51ret_a_imm : SDNode<"MCS51ISD::RET_A_IMM", SDT_MCS51RetA,
                             [SDNPHasChain]>;
def MCS51ret_zext8 : SDNode<"MCS51ISD::RET_ZEXT8", SDT_MCS51RetA,
                             [SDNPHasChain]>;
def MCS51ret_sext8 : SDNode<"MCS51ISD::RET_SEXT8", SDT_MCS51RetA,
                             [SDNPHasChain]>;
def MCS51retinterrupt : SDNode<"MCS51ISD::RET_INTERRUPT", SDTNone,
                               [SDNPHasChain]>;
def MCS51retword : SDNode<"MCS51ISD::RET_WORD", SDTNone, [SDNPHasChain]>;
def SDT_MCS51RetI32 : SDTypeProfile<0, 4, [SDTCisVT<0, i8>,
                                           SDTCisVT<1, i8>,
                                           SDTCisVT<2, i8>,
                                           SDTCisVT<3, i8>] >;
def MCS51ret_i32 : SDNode<"MCS51ISD::RET_I32", SDT_MCS51RetI32,
                          [SDNPHasChain]>;
def MCS51ret_i32_imm : SDNode<"MCS51ISD::RET_I32_IMM", SDT_MCS51RetI32,
                              [SDNPHasChain]>;
def SDT_MCS51RetI64 : SDTypeProfile<0, 8, [SDTCisVT<0, i8>,
                                          SDTCisVT<1, i8>,
                                          SDTCisVT<2, i8>,
                                          SDTCisVT<3, i8>,
                                          SDTCisVT<4, i8>,
                                          SDTCisVT<5, i8>,
                                          SDTCisVT<6, i8>,
                                          SDTCisVT<7, i8>] >;
def MCS51ret_i64 : SDNode<"MCS51ISD::RET_I64", SDT_MCS51RetI64,
                          [SDNPHasChain]>;
def SDT_MCS51ShiftWord8 : SDTypeProfile<1, 1, [SDTCisVT<0, i16>,
                                              SDTCisVT<1, i16>] >;
def SDT_MCS51ShiftByteImm : SDTypeProfile<1, 2, [SDTCisVT<0, i8>,
                                                SDTCisVT<1, i8>,
                                                SDTCisVT<2, i16>] >;
def SDT_MCS51ShiftByteReg : SDTypeProfile<1, 2, [SDTCisVT<0, i8>,
                                                SDTCisVT<1, i8>,
                                                SDTCisVT<2, i8>] >;
def MCS51sra8_imm : SDNode<"MCS51ISD::SRA8_IMM", SDT_MCS51ShiftByteImm>;
def MCS51sra8 : SDNode<"MCS51ISD::SRA8", SDT_MCS51ShiftByteImm>;
def MCS51srl8 : SDNode<"MCS51ISD::SRL8", SDT_MCS51ShiftByteImm>;
def MCS51shl8 : SDNode<"MCS51ISD::SHL8", SDT_MCS51ShiftByteImm>;
def MCS51sra8_reg : SDNode<"MCS51ISD::SRA8_REG", SDT_MCS51ShiftByteReg>;
def MCS51srl8_reg : SDNode<"MCS51ISD::SRL8_REG", SDT_MCS51ShiftByteReg>;
def MCS51shl8_reg : SDNode<"MCS51ISD::SHL8_REG", SDT_MCS51ShiftByteReg>;
def MCS51srl16_8 : SDNode<"MCS51ISD::SRL16_8", SDT_MCS51ShiftWord8>;
def MCS51shl16_8 : SDNode<"MCS51ISD::SHL16_8", SDT_MCS51ShiftWord8>;
def SDT_MCS51ShiftWord : SDTypeProfile<1, 2, [SDTCisVT<0, i16>,
                                              SDTCisVT<1, i16>,
                                              SDTCisVT<2, i16>] >;
def MCS51srl16 : SDNode<"MCS51ISD::SRL16", SDT_MCS51ShiftWord>;
def MCS51shl16 : SDNode<"MCS51ISD::SHL16", SDT_MCS51ShiftWord>;
def MCS51sra16 : SDNode<"MCS51ISD::SRA16", SDT_MCS51ShiftWord>;
def SDT_MCS51CmpUlt16 : SDTypeProfile<1, 2, [SDTCisVT<0, i8>,
                                            SDTCisVT<1, i16>,
                                            SDTCisVT<2, i16>] >;
def SDT_MCS51Cmp8 : SDTypeProfile<1, 2, [SDTCisVT<0, i8>,
                                         SDTCisVT<1, i8>,
                                         SDTCisVT<2, i8>] >;
def SDT_MCS51Cmp32 : SDTypeProfile<1, 5, [SDTCisVT<0, i8>,
                                         SDTCisVT<1, i16>,
                                         SDTCisVT<2, i16>,
                                         SDTCisVT<3, i16>,
                                         SDTCisVT<4, i16>,
                                         SDTCisVT<5, i8>] >;
def SDT_MCS51Add32 : SDTypeProfile<2, 4, [SDTCisVT<0, i16>,
                                         SDTCisVT<1, i16>,
                                         SDTCisVT<2, i16>,
                                         SDTCisVT<3, i16>,
                                         SDTCisVT<4, i16>,
                                         SDTCisVT<5, i16>] >;
def SDT_MCS51Add32Bytes : SDTypeProfile<4, 4, [SDTCisVT<0, i8>,
                                               SDTCisVT<1, i8>,
                                               SDTCisVT<2, i8>,
                                               SDTCisVT<3, i8>,
                                               SDTCisVT<4, i16>,
                                               SDTCisVT<5, i16>,
                                               SDTCisVT<6, i16>,
                                               SDTCisVT<7, i16>] >;
def SDT_MCS51Mul16 : SDTypeProfile<1, 2, [SDTCisVT<0, i16>,
                                          SDTCisVT<1, i16>,
                                          SDTCisVT<2, i16>] >;
def SDT_MCS51Shift32Parts : SDTypeProfile<2, 3, [SDTCisVT<0, i16>,
                                                 SDTCisVT<1, i16>,
                                                 SDTCisVT<2, i16>,
                                                 SDTCisVT<3, i16>,
                                                 SDTCisVT<4, i16>] >;
def MCS51cmpult16 : SDNode<"MCS51ISD::CMPULT16", SDT_MCS51CmpUlt16>;
def MCS51cmpuge16 : SDNode<"MCS51ISD::CMPUGE16", SDT_MCS51CmpUlt16>;
def MCS51cmpslt16 : SDNode<"MCS51ISD::CMPSLT16", SDT_MCS51CmpUlt16>;
def MCS51cmpsge16 : SDNode<"MCS51ISD::CMPSGE16", SDT_MCS51CmpUlt16>;
def MCS51cmpeq16 : SDNode<"MCS51ISD::CMPEQ16", SDT_MCS51CmpUlt16>;
def SDT_MCS51AddExtendedByteImmediate : SDTypeProfile<1, 2, [
  SDTCisVT<0, i16>, SDTCisVT<1, i8>, SDTCisVT<2, i16>
]>;
def MCS51add_zext8_imm : SDNode<"MCS51ISD::ADD_ZEXT8_IMM",
                                SDT_MCS51AddExtendedByteImmediate>;
def MCS51add_sext8_imm : SDNode<"MCS51ISD::ADD_SEXT8_IMM",
                                SDT_MCS51AddExtendedByteImmediate>;
def SDT_MCS51AddExtendedByte : SDTypeProfile<1, 2, [
  SDTCisVT<0, i16>, SDTCisVT<1, i16>, SDTCisVT<2, i8>
]>;
def MCS51add_zext8_16 : SDNode<"MCS51ISD::ADD_ZEXT8_16",
                               SDT_MCS51AddExtendedByte>;
def MCS51add_sext8_16 : SDNode<"MCS51ISD::ADD_SEXT8_16",
                               SDT_MCS51AddExtendedByte>;
def MCS51sub_zext8_16 : SDNode<"MCS51ISD::SUB_ZEXT8_16",
                               SDT_MCS51AddExtendedByte>;
def MCS51sub_sext8_16 : SDNode<"MCS51ISD::SUB_SEXT8_16",
                               SDT_MCS51AddExtendedByte>;
def MCS51cmpult8 : SDNode<"MCS51ISD::CMPULT8", SDT_MCS51Cmp8>;
def MCS51cmpuge8 : SDNode<"MCS51ISD::CMPUGE8", SDT_MCS51Cmp8>;
def MCS51cmpslt8 : SDNode<"MCS51ISD::CMPSLT8", SDT_MCS51Cmp8>;
def MCS51cmpsge8 : SDNode<"MCS51ISD::CMPSGE8", SDT_MCS51Cmp8>;
def MCS51cmpeq8 : SDNode<"MCS51ISD::CMPEQ8", SDT_MCS51Cmp8>;
def MCS51cmp32 : SDNode<"MCS51ISD::CMP32", SDT_MCS51Cmp32>;
def MCS51add32 : SDNode<"MCS51ISD::ADD32", SDT_MCS51Add32>;
def MCS51sub32 : SDNode<"MCS51ISD::SUB32", SDT_MCS51Add32>;
def MCS51add32_bytes : SDNode<"MCS51ISD::ADD32_BYTES",
                              SDT_MCS51Add32Bytes>;
def MCS51sub32_bytes : SDNode<"MCS51ISD::SUB32_BYTES",
                              SDT_MCS51Add32Bytes>;
def SDT_MCS51Add16Imm : SDTypeProfile<1, 2, [SDTCisVT<0, i16>,
                                              SDTCisVT<1, i16>,
                                              SDTCisVT<2, i16>] >;
def MCS51add16_imm : SDNode<"MCS51ISD::ADD16_IMM", SDT_MCS51Add16Imm>;
def SDT_MCS51Sub16Dec : SDTypeProfile<1, 1, [SDTCisVT<0, i16>,
                                              SDTCisVT<1, i16>] >;
def MCS51sub16_dec : SDNode<"MCS51ISD::SUB16_DEC", SDT_MCS51Sub16Dec>;
def MCS51mul16 : SDNode<"MCS51ISD::MUL16", SDT_MCS51Mul16>;
def SDT_MCS51Mul8To16 : SDTypeProfile<1, 2, [SDTCisVT<0, i16>,
                                             SDTCisVT<1, i8>,
                                             SDTCisVT<2, i8>] >;
def MCS51mul8to16 : SDNode<"MCS51ISD::MUL8_TO_16", SDT_MCS51Mul8To16>;
def MCS51srl32_parts : SDNode<"MCS51ISD::SRL32_PARTS",
                              SDT_MCS51Shift32Parts>;
def MCS51shl32_parts : SDNode<"MCS51ISD::SHL32_PARTS",
                              SDT_MCS51Shift32Parts>;
def MCS51sra32_parts : SDNode<"MCS51ISD::SRA32_PARTS",
                              SDT_MCS51Shift32Parts>;
def SDT_MCS51Select8 : SDTypeProfile<1, 3, [SDTCisVT<0, i8>,
                                           SDTCisVT<1, i8>,
                                           SDTCisVT<2, i8>,
                                           SDTCisVT<3, i8>] >;
def MCS51select8 : SDNode<"MCS51ISD::SELECT8", SDT_MCS51Select8>;
def SDT_MCS51SelectEq8 : SDTypeProfile<1, 4, [SDTCisVT<0, i8>,
                                             SDTCisVT<1, i8>,
                                             SDTCisVT<2, i8>,
                                             SDTCisVT<3, i8>,
                                             SDTCisVT<4, i8>] >;
def MCS51selecteq8 : SDNode<"MCS51ISD::SELECT_EQ8", SDT_MCS51SelectEq8>;
def SDT_MCS51SelectSign8 : SDTypeProfile<1, 3, [SDTCisVT<0, i8>,
                                               SDTCisVT<1, i8>,
                                               SDTCisVT<2, i8>,
                                               SDTCisVT<3, i8>] >;
def MCS51selectsign8 : SDNode<"MCS51ISD::SELECT_SIGN8",
                              SDT_MCS51SelectSign8>;
def SDT_MCS51Select16 : SDTypeProfile<1, 3, [SDTCisVT<0, i16>,
                                            SDTCisVT<1, i8>,
                                            SDTCisVT<2, i16>,
                                            SDTCisVT<3, i16>] >;
def MCS51select16 : SDNode<"MCS51ISD::SELECT16", SDT_MCS51Select16>;
def SDT_MCS51Call : SDTypeProfile<0, -1, [SDTCisVT<0, i16>]>;
def MCS51call : SDNode<"MCS51ISD::CALL", SDT_MCS51Call,
                       [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                        SDNPVariadic]>;
def MCS51call_i32 : SDNode<"MCS51ISD::CALL_I32", SDT_MCS51Call,
                           [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                            SDNPVariadic]>;
def MCS51call_i64 : SDNode<"MCS51ISD::CALL_I64", SDT_MCS51Call,
                           [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                            SDNPVariadic]>;
def MCS51icall : SDNode<"MCS51ISD::ICALL", SDT_MCS51Call,
                        [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                        SDNPVariadic]>;
def MCS51icall_i32 : SDNode<"MCS51ISD::ICALL_I32", SDT_MCS51Call,
                            [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                             SDNPVariadic]>;
def MCS51icall_i64 : SDNode<"MCS51ISD::ICALL_I64", SDT_MCS51Call,
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
// Glued to the call so the pops stay alive even when the call is a pure
// library call whose chain result the legalizer drops.
def MCS51poparg8 : SDNode<"MCS51ISD::POP_ARG8", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def MCS51pushpad8 : SDNode<"MCS51ISD::PUSH_PAD8", SDTNone, [SDNPHasChain]>;

// Brackets the pushes of stack arguments, so frame lowering knows how far SP
// has moved when it resolves a frame index inside the sequence.
def SDT_MCS51CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i16>,
                                            SDTCisVT<1, i16>]>;
def SDT_MCS51CallSeqEnd : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def MCS51callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_MCS51CallSeqStart,
                                [SDNPHasChain, SDNPOutGlue]>;
def MCS51callseq_end : SDNode<"ISD::CALLSEQ_END", SDT_MCS51CallSeqEnd,
                              [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

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
def RR_A : MCS51Inst<0x03, "rr a"> { let Defs = [A]; let Uses = [A]; }
def INC_A : MCS51Inst<0x04, "inc a"> {
  let Defs = [A]; let Uses = [A];
}
def RRC_A : MCS51Inst<0x13, "rrc a"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def DEC_A : MCS51Inst<0x14, "dec a"> {
  let Defs = [A];
  let Uses = [A];
}
def RL_A : MCS51Inst<0x23, "rl a"> { let Defs = [A]; let Uses = [A]; }
def RLC_A : MCS51Inst<0x33, "rlc a"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def MUL_AB : MCS51Inst<0xA4, "mul ab"> {
  let Defs = [A, B, C]; let Uses = [A, B];
  let hasSideEffects = 0;
}
let Defs = [A, B, C], Uses = [A, B] in
def DIV_AB : MCS51Inst<0x84, "div ab"> { let hasSideEffects = 0; }
def SWAP_A : MCS51Inst<0xC4, "swap a"> { let Defs = [A]; let Uses = [A]; }
def DA_A : MCS51Inst<0xD4, "da a"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def CLR_A : MCS51Inst<0xE4, "clr a"> {
  let Defs = [A]; let hasSideEffects = 0;
}
def CPL_A : MCS51Inst<0xF4, "cpl a"> { let Defs = [A]; let Uses = [A]; }
def CLR_C : MCS51Inst<0xC3, "clr c"> { let Defs = [C]; }
def CPL_C : MCS51Inst<0xB3, "cpl c"> { let Defs = [C]; let Uses = [C]; }
def SETB_C : MCS51Inst<0xD3, "setb c"> { let Defs = [C]; }
def JMP_ADPTR : MCS51Inst<0x73, "jmp @a+dptr"> {
  let isBranch = 1; let isIndirectBranch = 1;
  let isTerminator = 1; let isBarrier = 1;
  let Uses = [A, DPTR];
}
def MOVC_ADPTR : MCS51Inst<0x93, "movc a, @a+dptr"> {
  let Defs = [A]; let Uses = [A, DPTR]; let mayLoad = 1;
}
def INC_DPTR : MCS51Inst<0xA3, "inc dptr"> {
  let Defs = [DPTR]; let Uses = [DPTR]; let hasSideEffects = 0;
}
def MOVC_APC : MCS51Inst<0x83, "movc a, @a+pc"> {
  let Defs = [A]; let Uses = [A, PC]; let mayLoad = 1;
}
def MOVX_ADPTR : MCS51Inst<0xE0, "movx a, @dptr"> {
  let Defs = [A]; let Uses = [DPTR]; let mayLoad = 1;
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
def MOVX_AR0 : MCS51Inst<0xE2, "movx a, @r0"> {
  let Defs = [A]; let Uses = [R0]; let mayLoad = 1;
}
def MOVX_AR1 : MCS51Inst<0xE3, "movx a, @r1"> {
  let Defs = [A]; let Uses = [R1]; let mayLoad = 1;
}
def MOVX_DPTRA : MCS51Inst<0xF0, "movx @dptr, a"> {
  let Uses = [A, DPTR]; let mayStore = 1;
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
def RET_A : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$value);
  let AsmString = "";
  let Size = 2;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 0;
  let isReturn = 1;
  let isTerminator = 1;
}
def RET_A_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins i8imm:$value);
  let AsmString = "";
  let Size = 3;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let isReturn = 1;
  let isTerminator = 1;
}
class MCS51RetExt8Pseudo : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$value);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let isReturn = 1;
  let isTerminator = 1;
}
def RET_ZEXT8 : MCS51RetExt8Pseudo;
def RET_SEXT8 : MCS51RetExt8Pseudo;
def SRL16_8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SHL16_8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SRL16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src, MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SHL16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src, MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SRA16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src, MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def BUILDPAIR16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR8:$lo, MCS51GPR8:$hi);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SELECT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$cond, MCS51GPR8:$true,
                           MCS51GPR8:$false);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SELECT16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR8:$cond, MCS51GPR16:$true,
                           MCS51GPR16:$false);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def RET_I32 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$b0, MCS51GPR8:$b1,
                           MCS51GPR8:$b2, MCS51GPR8:$b3);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let isReturn = 1;
  let isTerminator = 1;
}
def MOVX_R0A : MCS51Inst<0xF2, "movx @r0, a"> {
  let Uses = [A, R0]; let mayStore = 1;
}
def MOVX_R1A : MCS51Inst<0xF3, "movx @r1, a"> {
  let Uses = [A, R1]; let mayStore = 1;
}
def MOV_A_R0_IND : MCS51Inst<0xE6, "mov a, @r0"> {
  let Defs = [A]; let Uses = [R0]; let mayLoad = 1;
}
def MOV_A_R1_IND : MCS51Inst<0xE7, "mov a, @r1"> {
  let Defs = [A]; let Uses = [R1]; let mayLoad = 1;
}
def MOV_R0_IND_A : MCS51Inst<0xF6, "mov @r0, a"> {
  let Uses = [A, R0]; let mayStore = 1;
}
def MOV_R1_IND_A : MCS51Inst<0xF7, "mov @r1, a"> {
  let Uses = [A, R1]; let mayStore = 1;
}
def INC_R0_IND : MCS51Inst<0x06, "inc @r0"> {
  let Uses = [R0]; let mayLoad = 1; let mayStore = 1;
}
def INC_R1_IND : MCS51Inst<0x07, "inc @r1"> {
  let Uses = [R1]; let mayLoad = 1; let mayStore = 1;
}
def DEC_R0_IND : MCS51Inst<0x16, "dec @r0"> {
  let Uses = [R0]; let mayLoad = 1; let mayStore = 1;
}
def DEC_R1_IND : MCS51Inst<0x17, "dec @r1"> {
  let Uses = [R1]; let mayLoad = 1; let mayStore = 1;
}
def ADD_A_R0_IND : MCS51Inst<0x26, "add a, @r0"> {
  let Defs = [A, C]; let Uses = [A, R0]; let mayLoad = 1;
}
def ADD_A_R1_IND : MCS51Inst<0x27, "add a, @r1"> {
  let Defs = [A, C]; let Uses = [A, R1]; let mayLoad = 1;
}
def ADDC_A_R0_IND : MCS51Inst<0x36, "addc a, @r0"> {
  let Defs = [A, C]; let Uses = [A, C, R0]; let mayLoad = 1;
}
def ADDC_A_R1_IND : MCS51Inst<0x37, "addc a, @r1"> {
  let Defs = [A, C]; let Uses = [A, C, R1]; let mayLoad = 1;
}
def SUBB_A_R0_IND : MCS51Inst<0x96, "subb a, @r0"> {
  let Defs = [A, C]; let Uses = [A, C, R0]; let mayLoad = 1;
}
def SUBB_A_R1_IND : MCS51Inst<0x97, "subb a, @r1"> {
  let Defs = [A, C]; let Uses = [A, C, R1]; let mayLoad = 1;
}
def ANL_A_R0_IND : MCS51Inst<0x56, "anl a, @r0"> {
  let Defs = [A]; let Uses = [A, R0]; let mayLoad = 1;
}
def ANL_A_R1_IND : MCS51Inst<0x57, "anl a, @r1"> {
  let Defs = [A]; let Uses = [A, R1]; let mayLoad = 1;
}
def ORL_A_R0_IND : MCS51Inst<0x46, "orl a, @r0"> {
  let Defs = [A]; let Uses = [A, R0]; let mayLoad = 1;
}
def ORL_A_R1_IND : MCS51Inst<0x47, "orl a, @r1"> {
  let Defs = [A]; let Uses = [A, R1]; let mayLoad = 1;
}
def XRL_A_R0_IND : MCS51Inst<0x66, "xrl a, @r0"> {
  let Defs = [A]; let Uses = [A, R0]; let mayLoad = 1;
}
def XRL_A_R1_IND : MCS51Inst<0x67, "xrl a, @r1"> {
  let Defs = [A]; let Uses = [A, R1]; let mayLoad = 1;
}
def RET  : MCS51Inst<0x22, "ret"> {
  let isReturn = 1;
  let isTerminator = 1;
}
def RET_NOA : MCS51Inst<0x22, "ret"> {
  let isReturn = 1;
  let isTerminator = 1;
  let isCodeGenOnly = 1;
}
def RETI : MCS51Inst<0x32, "reti"> {
  let isReturn = 1;
  let isTerminator = 1;
}

def : Pat<(MCS51retglue), (RET)>;
def : Pat<(MCS51ret_a MCS51GPR8:$value), (RET_A MCS51GPR8:$value)>;
def : Pat<(MCS51ret_a_imm imm:$value), (RET_A_IMM imm:$value)>;
def : Pat<(MCS51ret_zext8 MCS51GPR8:$value),
          (RET_ZEXT8 MCS51GPR8:$value)>;
def : Pat<(MCS51ret_sext8 MCS51GPR8:$value),
          (RET_SEXT8 MCS51GPR8:$value)>;
def : Pat<(MCS51retinterrupt), (RETI)>;
def : Pat<(MCS51retword), (RET16)>;
def : Pat<(MCS51ret_i32 MCS51GPR8:$b0, MCS51GPR8:$b1,
                         MCS51GPR8:$b2, MCS51GPR8:$b3),
          (RET_I32 MCS51GPR8:$b0, MCS51GPR8:$b1,
                   MCS51GPR8:$b2, MCS51GPR8:$b3)>;
def RET_I64 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$b0, MCS51GPR8:$b1,
                           MCS51GPR8:$b2, MCS51GPR8:$b3,
                           MCS51GPR8:$b4, MCS51GPR8:$b5,
                           MCS51GPR8:$b6, MCS51GPR8:$b7);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let isReturn = 1;
  let isTerminator = 1;
}
def : Pat<(MCS51ret_i64 MCS51GPR8:$b0, MCS51GPR8:$b1,
                        MCS51GPR8:$b2, MCS51GPR8:$b3,
                        MCS51GPR8:$b4, MCS51GPR8:$b5,
                        MCS51GPR8:$b6, MCS51GPR8:$b7),
          (RET_I64 MCS51GPR8:$b0, MCS51GPR8:$b1,
                   MCS51GPR8:$b2, MCS51GPR8:$b3,
                   MCS51GPR8:$b4, MCS51GPR8:$b5,
                   MCS51GPR8:$b6, MCS51GPR8:$b7)>;
def : Pat<(MCS51srl16_8 MCS51GPR16:$src), (SRL16_8 MCS51GPR16:$src)>;
def : Pat<(MCS51shl16_8 MCS51GPR16:$src), (SHL16_8 MCS51GPR16:$src)>;
def : Pat<(MCS51srl16 MCS51GPR16:$src, MCS51GPR16:$amount),
          (SRL16 MCS51GPR16:$src, MCS51GPR16:$amount)>;
def : Pat<(MCS51shl16 MCS51GPR16:$src, MCS51GPR16:$amount),
          (SHL16 MCS51GPR16:$src, MCS51GPR16:$amount)>;
def : Pat<(MCS51sra16 MCS51GPR16:$src, MCS51GPR16:$amount),
          (SRA16 MCS51GPR16:$src, MCS51GPR16:$amount)>;
def : Pat<(MCS51select8 MCS51GPR8:$cond, MCS51GPR8:$true,
                         MCS51GPR8:$false),
          (SELECT8 MCS51GPR8:$cond, MCS51GPR8:$true,
                   MCS51GPR8:$false)>;
def : Pat<(MCS51select16 MCS51GPR8:$cond, MCS51GPR16:$true,
                           MCS51GPR16:$false),
          (SELECT16 MCS51GPR8:$cond, MCS51GPR16:$true,
                    MCS51GPR16:$false)>;

def MCS51Imm8 : Operand<i8>,
                ImmLeaf<i8, "return isUInt<8>(Imm) || isInt<8>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm8OpValue";
  let DecoderMethod = "DecodeImm8";
}

def SELECT_EQ8ri : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51Imm8:$rhs,
                           MCS51GPR8:$tv, MCS51GPR8:$fv);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SELECT_EQ8rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs,
                           MCS51GPR8:$tv, MCS51GPR8:$fv);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def SELECT_SIGN8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$tv,
                           MCS51GPR8:$fv);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def : Pat<(MCS51selecteq8 MCS51GPR8:$lhs, MCS51Imm8:$rhs,
                           MCS51GPR8:$tv, MCS51GPR8:$fv),
          (SELECT_EQ8ri MCS51GPR8:$lhs, MCS51Imm8:$rhs,
                        MCS51GPR8:$tv, MCS51GPR8:$fv)>;
def : Pat<(MCS51selecteq8 MCS51GPR8:$lhs, MCS51GPR8:$rhs,
                           MCS51GPR8:$tv, MCS51GPR8:$fv),
          (SELECT_EQ8rr MCS51GPR8:$lhs, MCS51GPR8:$rhs,
                        MCS51GPR8:$tv, MCS51GPR8:$fv)>;
def : Pat<(MCS51selectsign8 MCS51GPR8:$lhs, MCS51GPR8:$tv,
                            MCS51GPR8:$fv),
          (SELECT_SIGN8 MCS51GPR8:$lhs, MCS51GPR8:$tv,
                        MCS51GPR8:$fv)>;

def RET_I32_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$b0, MCS51Imm8:$b1,
                           MCS51Imm8:$b2, MCS51Imm8:$b3);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let isReturn = 1;
  let isTerminator = 1;
}

def : Pat<(MCS51ret_i32_imm MCS51Imm8:$b0, MCS51Imm8:$b1,
                             MCS51Imm8:$b2, MCS51Imm8:$b3),
          (RET_I32_IMM MCS51Imm8:$b0, MCS51Imm8:$b1,
                       MCS51Imm8:$b2, MCS51Imm8:$b3)>;

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
}

def MCS51Addr16BE : Operand<OtherVT> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm16BEOpValue";
  let DecoderMethod = "DecodeImm16BE";
}

def MCS51Addr11 : Operand<OtherVT> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getAddr11OpValue";
  let DecoderMethod = "DecodeAddr11";
}

def MCS51Imm16 : Operand<i16>,
                 ImmLeaf<i16, "return isUInt<16>(Imm) || isInt<16>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm16OpValue";
}

def MCS51Imm16BE : Operand<i16>,
                   ImmLeaf<i16, "return isUInt<16>(Imm) || isInt<16>(Imm);"> {
  let ParserMatchClass = ImmAsmOperand;
  let EncoderMethod = "getImm16BEOpValue";
  let DecoderMethod = "DecodeImm16BE";
}

def MCS51Imm16NotOne : Operand<i16>,
                       ImmLeaf<i16, "return isUInt<16>(Imm) || isInt<16>(Imm);">;

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
  let mayLoad = 1;
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

def MOV_C_BIT : MCS51BitInst<0xA2, "mov c, $bitaddr"> {
  let Defs = [C]; let mayLoad = 1;
}
def MOV_BIT_C : MCS51BitInst<0x92, "mov $bitaddr, c"> {
  let Uses = [C]; let mayStore = 1;
}
def CLR_BIT : MCS51BitInst<0xC2, "clr $bitaddr"> { let mayStore = 1; }
def SETB_BIT : MCS51BitInst<0xD2, "setb $bitaddr"> { let mayStore = 1; }
def CPL_BIT : MCS51BitInst<0xB2, "cpl $bitaddr"> {
  let mayLoad = 1; let mayStore = 1;
}
def ANL_C_BIT : MCS51BitInst<0x82, "anl c, $bitaddr"> {
  let Defs = [C]; let Uses = [C]; let mayLoad = 1;
}
def ANL_C_NOTBIT : MCS51BitInst<0xB0, "anl c, /$bitaddr"> {
  let Defs = [C]; let Uses = [C]; let mayLoad = 1;
}
def ORL_C_BIT : MCS51BitInst<0x72, "orl c, $bitaddr"> {
  let Defs = [C]; let Uses = [C]; let mayLoad = 1;
}
def ORL_C_NOTBIT : MCS51BitInst<0xA0, "orl c, /$bitaddr"> {
  let Defs = [C]; let Uses = [C]; let mayLoad = 1;
}

def XCH_A_R0_IND : MCS51Inst<0xC6, "xch a, @r0"> {
  let Defs = [A]; let Uses = [A]; let mayLoad = 1; let mayStore = 1;
}
def XCH_A_R1_IND : MCS51Inst<0xC7, "xch a, @r1"> {
  let Defs = [A]; let Uses = [A]; let mayLoad = 1; let mayStore = 1;
}
def XCHD_A_R0_IND : MCS51Inst<0xD6, "xchd a, @r0"> {
  let Defs = [A]; let Uses = [A]; let mayLoad = 1; let mayStore = 1;
}
def XCHD_A_R1_IND : MCS51Inst<0xD7, "xchd a, @r1"> {
  let Defs = [A]; let Uses = [A]; let mayLoad = 1; let mayStore = 1;
}

class MCS51DJNZReg<bits<5> opcode> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$rn, MCS51Rel8:$target);
  let AsmString = "djnz $rn, $target";
  let Size = 2;
  let isBranch = 1;
  let isTerminator = 1;
  let Constraints = "$dst = $rn";
  bits<3> dst;
  bits<3> rn;
  bits<8> target;
  bits<16> Inst;
  let Inst{7-3} = opcode;
  let Inst{2-0} = dst;
  let Inst{15-8} = target;
}

def DJNZ_DIRECT : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51Rel8:$target);
  let AsmString = "djnz $addr, $target";
  let Size = 3;
  let isBranch = 1;
  let isTerminator = 1;
  let mayLoad = 1;
  let mayStore = 1;
  let Defs = [B];
  let Uses = [B];
  bits<8> addr;
  bits<8> target;
  bits<24> Inst;
  let Inst{7-0} = 0xD5;
  let Inst{15-8} = addr;
  let Inst{23-16} = target;
}

def DJNZ_RN : MCS51DJNZReg<0b11011>;

def CJNE_A_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$imm, MCS51Rel8:$target);
  let AsmString = "cjne a, #$imm, $target";
  let Size = 3;
  let Defs = [C];
  let Uses = [A];
  let isBranch = 1;
  let isTerminator = 1;
  bits<8> imm; bits<8> target; bits<24> Inst;
  let Inst{7-0} = 0xB4; let Inst{15-8} = imm; let Inst{23-16} = target;
}
def CJNE_A_DIRECT : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51Rel8:$target);
  let AsmString = "cjne a, $addr, $target";
  let Size = 3;
  let Defs = [C];
  let Uses = [A, DPTR, B];
  let mayLoad = 1;
  let isBranch = 1;
  let isTerminator = 1;
  bits<8> addr; bits<8> target; bits<24> Inst;
  let Inst{7-0} = 0xB5; let Inst{15-8} = addr; let Inst{23-16} = target;
}
class MCS51CJNE_IND<bits<8> opcode, string reg, Register Ptr> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$imm, MCS51Rel8:$target);
  let AsmString = "cjne " # reg # ", #$imm, $target";
  let Size = 3;
  let Defs = [C];
  let Uses = [Ptr];
  let mayLoad = 1;
  let isBranch = 1;
  let isTerminator = 1;
  bits<8> imm; bits<8> target; bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm; let Inst{23-16} = target;
}
def CJNE_IND_R0 : MCS51CJNE_IND<0xB6, "@r0", R0>;
def CJNE_IND_R1 : MCS51CJNE_IND<0xB7, "@r1", R1>;
class MCS51CJNE_RN<bits<8> opcode> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$rn, MCS51Imm8:$imm,
                           MCS51Rel8:$target);
  let AsmString = "cjne $rn, #$imm, $target";
  let Size = 3;
  let Defs = [C];
  let isBranch = 1;
  let isTerminator = 1;
  bits<3> rn; bits<8> imm; bits<8> target; bits<24> Inst;
  let Inst{7-3} = opcode{7-3}; let Inst{2-0} = rn;
  let Inst{15-8} = imm; let Inst{23-16} = target;
}
def CJNE_RN : MCS51CJNE_RN<0xB8>;

let isBarrier = 1 in
def SJMP : MCS51RelBranch<0x80, "sjmp">;
def JZ : MCS51RelBranch<0x60, "jz"> { let Uses = [A]; }
def JNZ : MCS51RelBranch<0x70, "jnz"> { let Uses = [A]; }
def JC : MCS51RelBranch<0x40, "jc"> { let Uses = [C]; }
def JNC : MCS51RelBranch<0x50, "jnc"> { let Uses = [C]; }
def JB : MCS51BitRelBranch<0x20, "jb">;
def JNB : MCS51BitRelBranch<0x30, "jnb">;
def JBC : MCS51BitRelBranch<0x10, "jbc"> { let mayStore = 1; }

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

// A 16-bit compare and branch in one pseudo: kind 2 equal, 5 not equal,
// 0 and 1 unsigned and signed less than, 3 and 4 unsigned and signed greater
// or equal. Expanded over the bytes of the operands.
def SDT_MCS51BrCmp16 : SDTypeProfile<0, 4, [SDTCisVT<0, i16>,
                                            SDTCisSameAs<0, 1>,
                                            SDTCisVT<2, i8>,
                                            SDTCisVT<3, OtherVT>]>;
def MCS51brcmp16 : SDNode<"MCS51ISD::BR_CMP16", SDT_MCS51BrCmp16,
                          [SDNPHasChain]>;
def BRCMP16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR16:$lhs, MCS51GPR16:$rhs, MCS51Imm8:$kind,
                           MCS51Addr16:$target);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let isBranch = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [A, B, C];
  let Pattern = [(MCS51brcmp16 MCS51GPR16:$lhs, MCS51GPR16:$rhs, imm:$kind,
                               bb:$target)];
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
  let InOperandList = (ins MCS51GPR16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

// Absolute XDATA accesses must load the symbol/address into DPTR before using
// MOVX. Keep these separate from LOADX8/LOADX16, which operate on a live pointer
// already held in DPTR.
def LOADXABS8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Imm16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, DPTR];
}

def LOADXABS16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51Imm16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, DPTR];
}

def LOADX16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$addr);
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
  let InOperandList = (ins MCS51GPR16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADCODEABS8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Imm16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, DPTR];
}

def LOADCODE16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
}

def LOADCODEABS16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51Imm16:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, DPTR];
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
  let OutOperandList = (outs MCS51GPR16:$dst);
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
  let InOperandList = (ins MCS51Direct8:$addr, MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
}

def LOADBIT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, C];
}

def STOREBIT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, C];
}

def STOREX8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR16:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
}

def STOREX16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR16:$addr, MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
}

def STOREXABS8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm16:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, DPTR];
}
def STOREXABS8_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm16:$addr, MCS51Imm8:$value);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, DPTR];
}
def STOREXABS16_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm16:$addr, MCS51Imm16:$value);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, DPTR];
}

def STOREXABS16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm16:$addr, MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, DPTR];
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
def STOREI8 : MCS51IndirectMemPseudo<0> {
  let usesCustomInserter = 0;
}
def LOADP8 : MCS51IndirectMemPseudo<1>;
def STOREP8 : MCS51IndirectMemPseudo<0>;

def LOADIDATA_GLOBAL8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Imm8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, R0];
}

def STOREIDATA_GLOBAL8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, R0];
}

def STOREIDATA_GLOBAL8_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$addr, MCS51Imm8:$imm);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [R0];
}

def LOADPDATA_GLOBAL8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Imm8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, R0];
}

def STOREPDATA_GLOBAL8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$addr, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, R0];
}

def LOADPDATA_GLOBAL16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51Imm8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, DPTR, R0];
}

def STOREPDATA_GLOBAL16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$addr, MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, R0];
}

def LOADIDATA_GLOBAL16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51Imm8:$addr);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let Defs = [A, DPTR, R0];
}

def STOREIDATA_GLOBAL16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$addr, MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let mayStore = 1;
  let Defs = [A, R0];
}

class MCS51IndirectWordPseudo<bits<1> IsLoad> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = !if(IsLoad, (outs MCS51GPR16:$dst), (outs));
  let InOperandList = !if(IsLoad,
                          (ins MCS51Indirect8:$addr),
                          (ins MCS51Indirect8:$addr, MCS51GPR16:$src));
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

def LJMP : MCS51Abs16Inst<0x02, "ljmp", MCS51Addr16BE> {
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
}

class MCS51Abs11Inst<bits<5> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Addr11:$addr);
  let AsmString = mnemonic # " $addr";
  let Size = 2;
  bits<11> addr;
  bits<16> Inst;
  let Inst{4-0} = opcode;
  let Inst{7-5} = addr{10-8};
  let Inst{15-8} = addr{7-0};
}

def AJMP : MCS51Abs11Inst<0b00001, "ajmp"> {
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
}
let isPseudo = 1, isCodeGenOnly = 1, hasSideEffects = 1, Size = 0,
    Defs = [SP], Uses = [SP] in {
def ADJCALLSTACKDOWN : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins i16imm:$amt1, i16imm:$amt2);
  let AsmString = "";
  let Pattern = [(MCS51callseq_start timm:$amt1, timm:$amt2)];
}
def ADJCALLSTACKUP : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins i16imm:$amt1, i16imm:$amt2);
  let AsmString = "";
  let Pattern = [(MCS51callseq_end timm:$amt1, timm:$amt2)];
}
}

// A call may change the caller-saved imaginary registers IM0-IM11; IM12-IM23
// are preserved by callees.
defvar ImagRegs = !foreach(i, !filter(j, !range(24), !or(!lt(j, 8), !gt(j, 23))),
                           !cast<Register>("IM" # i));
let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1], ImagRegs),
    Uses = [SP], isCall = 1 in
def ACALL : MCS51Abs11Inst<0b10001, "acall">;
let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1], ImagRegs),
    Uses = [SP], isCall = 1 in
def LCALL : MCS51Abs16Inst<0x12, "lcall", MCS51Imm16BE> {
  let Pattern = [(MCS51call (i16 tglobaladdr:$addr))];
}

def : Pat<(MCS51call (i16 texternalsym:$addr)),
          (LCALL texternalsym:$addr)>;

let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1, R4, R5, R6, R7], ImagRegs),
    Uses = [SP], isCall = 1, isCodeGenOnly = 1 in
def LCALL_I32 : MCS51Abs16Inst<0x12, "lcall", MCS51Imm16BE> {
  let Pattern = [(MCS51call_i32 (i16 tglobaladdr:$addr))];
}
def : Pat<(MCS51call_i32 (i16 texternalsym:$addr)),
          (LCALL_I32 texternalsym:$addr)>;

let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1, R2, R3, R4, R5, R6, R7], ImagRegs),
    Uses = [SP], isCall = 1, isCodeGenOnly = 1 in
def LCALL_I64 : MCS51Abs16Inst<0x12, "lcall", MCS51Imm16BE> {
  let Pattern = [(MCS51call_i64 (i16 tglobaladdr:$addr))];
}
def : Pat<(MCS51call_i64 (i16 texternalsym:$addr)),
          (LCALL_I64 texternalsym:$addr)>;

// The 8051 has no indirect CALL instruction. The custom inserter expands this
// pseudo to an LCALL of a local dispatch block and a continuation jump when
// needed. The dispatcher clears A, then uses JMP @A+DPTR to tail-transfer to
// the callee; its RET returns to the call site's continuation.
let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1], ImagRegs),
    Uses = [SP, DPTR], isCall = 1 in
def ICALL : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "";
  // AsmPrinter expands this to two PUSH instructions and an LCALL.
  let Size = 7;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// The selected form takes the target in a word register; the inserter moves it
// into DPTR and emits ICALL.
let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1], ImagRegs), Uses = [SP],
    isCall = 1, isPseudo = 1, isCodeGenOnly = 1, usesCustomInserter = 1 in
def ICALL_W : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR16:$target);
  let AsmString = "";
  let Size = 7;
}
def : Pat<(MCS51icall (i16 MCS51GPR16:$target)), (ICALL_W MCS51GPR16:$target)>;

let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1, R4, R5, R6, R7], ImagRegs),
    Uses = [SP, DPTR], isCall = 1, isPseudo = 1, isCodeGenOnly = 1,
    usesCustomInserter = 1 in
def ICALL_I32 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR16:$target);
  let AsmString = "";
  let Size = 7;
}
def : Pat<(MCS51icall_i32 (i16 MCS51GPR16:$target)),
          (ICALL_I32 MCS51GPR16:$target)>;

let Defs = !listconcat([A, B, C, DPTR, SP, R0, R1, R2, R3, R4, R5, R6, R7], ImagRegs),
    Uses = [SP, DPTR], isCall = 1, isPseudo = 1, isCodeGenOnly = 1,
    usesCustomInserter = 1 in
def ICALL_I64 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR16:$target);
  let AsmString = "";
  let Size = 7;
}
def : Pat<(MCS51icall_i64 (i16 MCS51GPR16:$target)),
          (ICALL_I64 MCS51GPR16:$target)>;

def MOV_DPTR_IMM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51PTR:$dst);
  let InOperandList = (ins MCS51Imm16BE:$imm);
  let AsmString = "mov dptr, #$imm";
  let Size = 3;
  let hasSideEffects = 0;
  let isReMaterializable = 1;
  bits<1> dst;
  let dst = 0;
  bits<16> imm;
  bits<24> Inst;
  let Inst{7-0} = 0x90;
  let Inst{23-8} = imm;
}
// A 16-bit constant in DPTR or a register pair. Expanded after allocation.
def LDI16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51Imm16BE:$imm);
  let AsmString = "";
  let Size = 4;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let hasSideEffects = 0;
  let isReMaterializable = 1;
  let isAsCheapAsAMove = 1;
  let Pattern = [(set MCS51GPR16:$dst, MCS51Imm16BE:$imm)];
}

def : Pat<(i16 tglobaladdr:$addr), (LDI16 tglobaladdr:$addr)>;

def LOADSTACKARG8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Imm8:$spadjust);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1, C];
  let Uses = [SP];
}

def LOADSTACKARG16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
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
  let InOperandList = (ins MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [SP];
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

// The expansions keep A and the carry flag intact: they either walk R1 with
// INC/DEC or save and restore them around forming the address.
def SPILL_LOAD8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [R1];
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
  let Defs = [R1];
  let Uses = [SP];
}

def SPILL_LOAD_IM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51Imag8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [R1];
  let Uses = [SP];
}
def SPILL_STORE_IM : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51Imag8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [R1];
  let Uses = [SP];
}

def SPILL_LOAD_INDIRECT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51Indirect8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [R1];
  let Uses = [SP];
}

def SPILL_STORE_INDIRECT8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51Indirect8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [R1];
  let Uses = [SP];
}

def SPILL_LOAD16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [R1];
  let Uses = [SP];
}

def SPILL_LOAD_A8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1];
  let Uses = [SP];
}

def SPILL_STORE16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [R1];
  let Uses = [SP];
}

def SPILL_STORE_A8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51AReg:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  // The src operand keeps A live; the expansion preserves both A and PSW.
  let Defs = [R1, SP];
  let Uses = [PSW, R1, SP];
}

// Frame-address and frame-memory pseudos also calculate SP + offset with ADD.
def FRAMEADDR_R1 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Defs = [A, R1, C];
  let Uses = [SP];
}

def FRAMEADDR8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Defs = [A, R1, C];
  let Uses = [SP];
}

def FRAMEADDR8_INDEX : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR8:$index);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let hasSideEffects = 1;
  let Defs = [A, R1, C];
  let Uses = [SP];
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
  let Defs = [A, R1, C];
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
  let Defs = [A, R1, C];
  let Uses = [SP];
}

def LOAD_FRAME16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1, C];
  let Uses = [SP];
}

def STORE_FRAME16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [A, R1, C];
  let Uses = [SP];
}

def LOAD_FRAME8_INDEX : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR16:$index);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayLoad = 1;
  let Defs = [A, R1, C];
  let Uses = [DPTR, SP];
}

def STORE_FRAME8_INDEX : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51FrameIndex:$fi, MCS51Imm8:$offset,
                           MCS51GPR16:$index, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let mayStore = 1;
  let Defs = [A, R1, C];
  let Uses = [DPTR, SP];
}


// In-place word increment and decrement. The AsmPrinter expands them over
// the two bytes of the pair: INC lo; MOV A,lo; JNZ skip; INC hi.
let Constraints = "$dst = $src", isPseudo = 1, isCodeGenOnly = 1,
    hasSideEffects = 0, Defs = [A, C] in {
def INC16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src);
  let AsmString = "";
  let Size = 8;
}
def DEC16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src);
  let AsmString = "";
  let Size = 9;
  let Pattern = [(set MCS51GPR16:$dst, (MCS51sub16_dec MCS51GPR16:$src))];
}
}

// Counted shift loops, expanded by the AsmPrinter over the direct addresses of
// the operand bytes: L: shift; DJNZ count,L. The value and the counter are
// updated in place.
let isPseudo = 1, isCodeGenOnly = 1, hasSideEffects = 0, Defs = [A, C] in {
class MCS51ShiftLoop16<int size> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst, MCS51GPR8:$cntout);
  let InOperandList = (ins MCS51GPR16:$src, MCS51GPR8:$cnt);
  let AsmString = "";
  let Size = size;
  let Constraints = "$dst = $src, $cntout = $cnt";
}
class MCS51ShiftLoop8<int size> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51Imag8:$dst, MCS51GPR8:$cntout);
  let InOperandList = (ins MCS51Imag8:$src, MCS51GPR8:$cnt);
  let AsmString = "";
  let Size = size;
  let Constraints = "$dst = $src, $cntout = $cnt";
}
class MCS51ShiftLoop32<int size> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dlo, MCS51GPR16:$dhi,
                             MCS51GPR8:$cntout);
  let InOperandList = (ins MCS51GPR16:$slo, MCS51GPR16:$shi,
                           MCS51GPR8:$cnt);
  let AsmString = "";
  let Size = size;
  let Constraints = "$dlo = $slo, $dhi = $shi, $cntout = $cnt";
}
def SHL32_LOOP : MCS51ShiftLoop32<23>;
def SRL32_LOOP : MCS51ShiftLoop32<23>;
def SRA32_LOOP : MCS51ShiftLoop32<24>;
def SHL16_LOOP : MCS51ShiftLoop16<13>;
def SRL16_LOOP : MCS51ShiftLoop16<13>;
def SRA16_LOOP : MCS51ShiftLoop16<14>;
def SHL8_LOOP : MCS51ShiftLoop8<8>;
def SRL8_LOOP : MCS51ShiftLoop8<8>;
def SRA8_LOOP : MCS51ShiftLoop8<9>;
}

def ADDDPTR16ri : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src, MCS51Imm16NotOne:$imm);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let Pattern = [(set MCS51GPR16:$dst,
                      (add MCS51GPR16:$src, MCS51Imm16NotOne:$imm))];
}

class MCS51WordArithmetic : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$lhs, MCS51GPR16:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}

class MCS51WordLogic<SDPatternOperator Op> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$lhs, MCS51GPR16:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst,
                      (Op MCS51GPR16:$lhs, MCS51GPR16:$rhs))];
}

def AND16rr : MCS51WordLogic<and>;
def OR16rr : MCS51WordLogic<or>;
def XOR16rr : MCS51WordLogic<xor>;

let Pattern = [(set MCS51GPR16:$dst, (add MCS51GPR16:$lhs, MCS51GPR16:$rhs))] in
def ADD16rr : MCS51WordArithmetic;
def ADD16ri : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$src, MCS51Imm16:$imm);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst,
                      (MCS51add16_imm MCS51GPR16:$src, MCS51Imm16:$imm))];
}
let Pattern = [(set MCS51GPR16:$dst, (sub MCS51GPR16:$lhs, MCS51GPR16:$rhs))] in
def SUB16rr : MCS51WordArithmetic;

let Pattern = [(set MCS51GPR16:$dst,
                    (MCS51mul16 MCS51GPR16:$lhs, MCS51GPR16:$rhs))] in
def MUL16rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$lhs, MCS51GPR16:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}

def MUL8TO16rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst,
                      (MCS51mul8to16 MCS51GPR8:$lhs, MCS51GPR8:$rhs))];
}

def SRL32_PARTS : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dstlo, MCS51GPR16:$dsthi);
  let InOperandList = (ins MCS51GPR16:$srclo, MCS51GPR16:$srchi,
                           MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dstlo, MCS51GPR16:$dsthi,
                      (MCS51srl32_parts MCS51GPR16:$srclo,
                                        MCS51GPR16:$srchi,
                                        MCS51GPR16:$amount))];
}
def SHL32_PARTS : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dstlo, MCS51GPR16:$dsthi);
  let InOperandList = (ins MCS51GPR16:$srclo, MCS51GPR16:$srchi,
                           MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dstlo, MCS51GPR16:$dsthi,
                      (MCS51shl32_parts MCS51GPR16:$srclo,
                                        MCS51GPR16:$srchi,
                                        MCS51GPR16:$amount))];
}
def SRA32_PARTS : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dstlo, MCS51GPR16:$dsthi);
  let InOperandList = (ins MCS51GPR16:$srclo, MCS51GPR16:$srchi,
                           MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dstlo, MCS51GPR16:$dsthi,
                      (MCS51sra32_parts MCS51GPR16:$srclo,
                                        MCS51GPR16:$srchi,
                                        MCS51GPR16:$amount))];
}

let Pattern = [(set MCS51GPR16:$dstlo, MCS51GPR16:$dsthi,
                    (MCS51add32 MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                                MCS51GPR16:$rhslo, MCS51GPR16:$rhshi))] in
def ADD32rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dstlo, MCS51GPR16:$dsthi);
  let InOperandList = (ins MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                           MCS51GPR16:$rhslo, MCS51GPR16:$rhshi);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}

let Pattern = [(set MCS51GPR16:$dstlo, MCS51GPR16:$dsthi,
                    (MCS51sub32 MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                                MCS51GPR16:$rhslo, MCS51GPR16:$rhshi))] in
def SUB32rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dstlo, MCS51GPR16:$dsthi);
  let InOperandList = (ins MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                           MCS51GPR16:$rhslo, MCS51GPR16:$rhshi);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}

def ADD32_BYTESrr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst0, MCS51GPR8:$dst1,
                             MCS51GPR8:$dst2, MCS51GPR8:$dst3);
  let InOperandList = (ins MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                           MCS51GPR16:$rhslo, MCS51GPR16:$rhshi);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst0, MCS51GPR8:$dst1,
                       MCS51GPR8:$dst2, MCS51GPR8:$dst3,
                       (MCS51add32_bytes MCS51GPR16:$lhslo,
                                         MCS51GPR16:$lhshi,
                                         MCS51GPR16:$rhslo,
                                         MCS51GPR16:$rhshi))];
}

def SUB32_BYTESrr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst0, MCS51GPR8:$dst1,
                             MCS51GPR8:$dst2, MCS51GPR8:$dst3);
  let InOperandList = (ins MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                           MCS51GPR16:$rhslo, MCS51GPR16:$rhshi);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst0, MCS51GPR8:$dst1,
                       MCS51GPR8:$dst2, MCS51GPR8:$dst3,
                       (MCS51sub32_bytes MCS51GPR16:$lhslo,
                                         MCS51GPR16:$lhshi,
                                         MCS51GPR16:$rhslo,
                                         MCS51GPR16:$rhshi))];
}

def CMP16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR16:$lhs, MCS51GPR16:$rhs,
                           MCS51Imm8:$signed);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
}
def CMP8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs,
                           MCS51Imm8:$kind);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}
def CMP8I : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51Imm8:$rhs,
                           MCS51Imm8:$kind);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}
def CMP32 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                           MCS51GPR16:$rhslo, MCS51GPR16:$rhshi,
                           MCS51Imm8:$kind);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
}
def : Pat<(MCS51cmpult16 MCS51GPR16:$lhs, MCS51GPR16:$rhs),
          (CMP16 MCS51GPR16:$lhs, MCS51GPR16:$rhs, 0)>;
def : Pat<(MCS51cmpslt16 MCS51GPR16:$lhs, MCS51GPR16:$rhs),
          (CMP16 MCS51GPR16:$lhs, MCS51GPR16:$rhs, 1)>;
def : Pat<(MCS51cmpuge16 MCS51GPR16:$lhs, MCS51GPR16:$rhs),
          (CMP16 MCS51GPR16:$lhs, MCS51GPR16:$rhs, 3)>;
def : Pat<(MCS51cmpsge16 MCS51GPR16:$lhs, MCS51GPR16:$rhs),
          (CMP16 MCS51GPR16:$lhs, MCS51GPR16:$rhs, 4)>;
def : Pat<(MCS51cmpeq16 MCS51GPR16:$lhs, MCS51GPR16:$rhs),
          (CMP16 MCS51GPR16:$lhs, MCS51GPR16:$rhs, 2)>;
def : Pat<(MCS51cmpult8 MCS51GPR8:$lhs, MCS51GPR8:$rhs),
          (CMP8 MCS51GPR8:$lhs, MCS51GPR8:$rhs, 0)>;
def : Pat<(MCS51cmpult8 MCS51GPR8:$lhs, MCS51Imm8:$rhs),
          (CMP8I MCS51GPR8:$lhs, MCS51Imm8:$rhs, 0)>;
def : Pat<(MCS51cmpslt8 MCS51GPR8:$lhs, MCS51GPR8:$rhs),
          (CMP8 MCS51GPR8:$lhs, MCS51GPR8:$rhs, 1)>;
def : Pat<(MCS51cmpuge8 MCS51GPR8:$lhs, MCS51GPR8:$rhs),
          (CMP8 MCS51GPR8:$lhs, MCS51GPR8:$rhs, 3)>;
def : Pat<(MCS51cmpuge8 MCS51GPR8:$lhs, MCS51Imm8:$rhs),
          (CMP8I MCS51GPR8:$lhs, MCS51Imm8:$rhs, 3)>;
def : Pat<(MCS51cmpsge8 MCS51GPR8:$lhs, MCS51GPR8:$rhs),
          (CMP8 MCS51GPR8:$lhs, MCS51GPR8:$rhs, 4)>;
def : Pat<(MCS51cmpeq8 MCS51GPR8:$lhs, MCS51GPR8:$rhs),
          (CMP8 MCS51GPR8:$lhs, MCS51GPR8:$rhs, 2)>;
def : Pat<(MCS51cmpeq8 MCS51GPR8:$lhs, MCS51Imm8:$rhs),
          (CMP8I MCS51GPR8:$lhs, MCS51Imm8:$rhs, 2)>;
def : Pat<(MCS51cmp32 MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                         MCS51GPR16:$rhslo, MCS51GPR16:$rhshi,
                         MCS51Imm8:$kind),
          (CMP32 MCS51GPR16:$lhslo, MCS51GPR16:$lhshi,
                 MCS51GPR16:$rhslo, MCS51GPR16:$rhshi,
                 MCS51Imm8:$kind)>;

// Start with the compact relative jump. The pre-emit branch relaxation pass
// promotes out-of-range branches to LJMP.
def : Pat<(br bb:$target), (SJMP MCS51Rel8:$target)>;

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
  let Defs = [A, C]; let Uses = [A];
}
def ADDC_A_IMM : MCS51AccImmInst<0x34, "addc"> {
  let Defs = [A, C]; let Uses = [A, C];
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
  let mayLoad = 1;
  let Defs = [A];
  let Uses = [A];
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
  let mayStore = 1;
  let Uses = [A];
  // Direct addresses 0x82 and 0x83 alias DPL and DPH. Conservatively model
  // direct stores as clobbering the 16-bit DPTR value.
  let Defs = [DPTR, B];
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
  let mayStore = 1;
  let Defs = [DPTR, B];
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
  let mayLoad = 1;
  let mayStore = 1;
  bits<8> addr;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
}

def MOV_DIRECT_DIRECT : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$dst, MCS51Direct8:$src);
  let AsmString = "mov $dst, $src";
  let Size = 3;
  let hasSideEffects = 1;
  let mayLoad = 1;
  let mayStore = 1;
  let Defs = [DPTR, B];
  let Uses = [B];
  bits<8> dst;
  bits<8> src;
  bits<24> Inst;
  let Inst{7-0} = 0x85;
  let Inst{15-8} = src;
  let Inst{23-16} = dst;
}

class MCS51IndirectImmInst<bits<8> opcode, string mnemonic,
                           Register Ptr> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imm8:$imm);
  let AsmString = mnemonic # " #$imm";
  let Size = 2;
  let hasSideEffects = 1;
  let Uses = [Ptr];
  let mayStore = 1;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
}

def MOV_A_DIRECT : MCS51AccDirectInst<0xE5, "mov"> {
  let Defs = [];
  let Uses = [];
}
def XCH_A_DIRECT : MCS51AccDirectInst<0xC5, "xch"> { let mayStore = 1; }
def ADD_A_DIRECT : MCS51AccDirectInst<0x25, "add"> {
  let Defs = [A, C]; let Uses = [A];
}
def ADDC_A_DIRECT : MCS51AccDirectInst<0x35, "addc"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def SUBB_A_DIRECT : MCS51AccDirectInst<0x95, "subb"> {
  let Defs = [A, C]; let Uses = [A, C];
}
def ANL_A_DIRECT : MCS51AccDirectInst<0x55, "anl"> {
  let Defs = [A]; let Uses = [A];
}
def ORL_A_DIRECT : MCS51AccDirectInst<0x45, "orl"> {
  let Defs = [A]; let Uses = [A];
}
def XRL_A_DIRECT : MCS51AccDirectInst<0x65, "xrl"> {
  let Defs = [A]; let Uses = [A];
}

class MCS51AccIndirectInst<bits<8> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51Indirect8:$ri);
  let AsmString = mnemonic # " a, @$ri";
  let Size = 1;
  let hasSideEffects = 1;
  let mayLoad = 1;
  let Defs = [A];
  let Uses = [A];
  bits<1> dst;
  let dst = 0;
  bits<1> ri;
  bits<8> Inst;
  let Inst{7-1} = opcode{7-1};
  let Inst{0} = ri;
}

def ADD_A_IND_RI : MCS51AccIndirectInst<0x26, "add"> {
  let Defs = [A, C];
}
def SUBB_A_IND_RI : MCS51AccIndirectInst<0x96, "subb"> {
  let Defs = [A, C];
  let Uses = [A, C];
}
def ANL_A_IND_RI : MCS51AccIndirectInst<0x56, "anl">;
def ORL_A_IND_RI : MCS51AccIndirectInst<0x46, "orl">;
def XRL_A_IND_RI : MCS51AccIndirectInst<0x66, "xrl">;
def MOV_DIRECT_A : MCS51DirectAccInst<0xF5, "mov">;
// Precise implicit register effects for the SFR moves used by wide
// arithmetic. The generic direct form conservatively clobbers B and DPTR.
class MCS51FixedSFRStore<bits<8> address, string mnemonic,
                         Register DefReg> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = mnemonic;
  let Size = 2;
  let hasSideEffects = 1;
  let Defs = [DefReg];
  let Uses = [A];
  bits<16> Inst;
  let Inst{7-0} = 0xF5;
  let Inst{15-8} = address;
}
class MCS51FixedSFRLoad<bits<8> address, string mnemonic,
                        Register UseReg> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins);
  let AsmString = mnemonic;
  let Size = 2;
  let hasSideEffects = 1;
  let Defs = [];
  let Uses = [UseReg];
  bits<1> dst;
  let dst = 0;
  bits<16> Inst;
  let Inst{7-0} = 0xE5;
  let Inst{15-8} = address;
}
def MOV_DPL_A : MCS51FixedSFRStore<0x82, "mov 130, a", DPL>;
def MOV_DPH_A : MCS51FixedSFRStore<0x83, "mov 131, a", DPH>;
// Byte copies between a register and one half of DPTR. They touch only that
// half, unlike the generic direct moves that model a write to all of DPTR.
class MCS51DptrHalfFromRn<bits<8> address, string mnemonic, Register Half>
    : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "mov " # mnemonic # ", $src";
  let Size = 2;
  let hasSideEffects = 1;
  let isCodeGenOnly = 1;
  let Defs = [Half];
  bits<3> src;
  bits<16> Inst;
  let Inst{7-3} = 0b10001;
  let Inst{2-0} = src;
  let Inst{15-8} = address;
}
class MCS51RnFromDptrHalf<bits<8> address, string mnemonic, Register Half>
    : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins);
  let AsmString = "mov $dst, " # mnemonic;
  let Size = 2;
  let hasSideEffects = 1;
  let isCodeGenOnly = 1;
  let Uses = [Half];
  bits<3> dst;
  bits<16> Inst;
  let Inst{7-3} = 0b10101;
  let Inst{2-0} = dst;
  let Inst{15-8} = address;
}
def MOV_DPL_RN : MCS51DptrHalfFromRn<0x82, "130", DPL>;
def MOV_DPH_RN : MCS51DptrHalfFromRn<0x83, "131", DPH>;
def MOV_RN_DPL : MCS51RnFromDptrHalf<0x82, "130", DPL>;
def MOV_RN_DPH : MCS51RnFromDptrHalf<0x83, "131", DPH>;
class MCS51DptrByteLoad<bits<8> address, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51AReg:$dst);
  let InOperandList = (ins MCS51PTR:$dptr);
  let AsmString = mnemonic;
  let Size = 2;
  let hasSideEffects = 1;
  let mayLoad = 1;
  let Defs = [];
  bits<1> dst;
  let dst = 0;
  // The source operand models the DPTR value for register allocation. Its
  // physical encoding is fixed by the SFR address below.
  bits<4> dptr;
  let dptr = 0;
  bits<16> Inst;
  let Inst{7-0} = 0xE5;
  let Inst{15-8} = address;
}
def MOV_A_DPL : MCS51DptrByteLoad<0x82, "mov a, 130">;
def MOV_A_DPH : MCS51DptrByteLoad<0x83, "mov a, 131">;
def MOV_SP_A : MCS51FixedSFRStore<0x81, "mov 129, a", SP>;
def MOV_B_A : MCS51FixedSFRStore<0xF0, "mov 240, a", B>;
def MOV_A_B : MCS51FixedSFRLoad<0xF0, "mov a, 240", B>;
def MOV_A_SP : MCS51FixedSFRLoad<0x81, "mov a, 129", SP>;
def ANL_DIRECT_A : MCS51DirectAccInst<0x52, "anl"> { let mayLoad = 1; }
def ORL_DIRECT_A : MCS51DirectAccInst<0x42, "orl"> { let mayLoad = 1; }
def XRL_DIRECT_A : MCS51DirectAccInst<0x62, "xrl"> { let mayLoad = 1; }
def MOV_DIRECT_IMM : MCS51DirectImmInst<0x75, "mov">;
def ANL_DIRECT_IMM : MCS51DirectImmInst<0x53, "anl"> { let mayLoad = 1; }
def ORL_DIRECT_IMM : MCS51DirectImmInst<0x43, "orl"> { let mayLoad = 1; }
def XRL_DIRECT_IMM : MCS51DirectImmInst<0x63, "xrl"> { let mayLoad = 1; }
def PUSH_DIRECT : MCS51DirectInst<0xC0, "push"> {
  let Defs = [SP]; let Uses = [SP];
}
def POP_DIRECT : MCS51DirectInst<0xD0, "pop"> {
  let Defs = [SP, A]; let Uses = [SP];
}
// Releases one stack argument byte after a call. DEC SP has the same size as
// POP direct but leaves every register untouched.
def POP_ARG_SP : MCS51DirectInst<0x15, "dec"> {
  let isCodeGenOnly = 1;
  let Defs = [SP]; let Uses = [SP];
}
def PUSH_PSW : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "push 208";
  let Size = 2;
  let hasSideEffects = 1;
  let Defs = [SP];
  let Uses = [SP];
  bits<16> Inst;
  let Inst{7-0} = 0xC0;
  let Inst{15-8} = 0xD0;
}
def POP_PSW : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "pop 208";
  let Size = 2;
  let hasSideEffects = 1;
  let Defs = [SP];
  let Uses = [SP];
  bits<16> Inst;
  let Inst{7-0} = 0xD0;
  let Inst{15-8} = 0xD0;
}
def INC_DIRECT : MCS51DirectInst<0x05, "inc">;
def DEC_DIRECT : MCS51DirectInst<0x15, "dec">;
def MOV_R0_IND_IMM : MCS51IndirectImmInst<0x76, "mov @r0,", R0>;
def MOV_R1_IND_IMM : MCS51IndirectImmInst<0x77, "mov @r1,", R1>;

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

def MOV_RN_DIRECT : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = "mov $dst, $addr";
  let Size = 2;
  let mayLoad = 1;
  bits<3> dst; bits<8> addr; bits<16> Inst;
  let Inst{7-3} = 0b10101;
  let Inst{2-0} = dst;
  let Inst{15-8} = addr;
}

def MOV_DIRECT_RN : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr, MCS51GPR8:$src);
  let AsmString = "mov $addr, $src";
  let Size = 2;
  let mayStore = 1;
  let Defs = [DPTR, B];
  bits<3> src; bits<8> addr; bits<16> Inst;
  let Inst{7-3} = 0b10001;
  let Inst{2-0} = src;
  let Inst{15-8} = addr;
}

// Moves involving imaginary registers. They are ordinary direct-addressed
// moves; the imaginary register operand supplies the direct address, so the
// allocator sees exactly which byte of DATA RAM is read or written.
class MCS51ImagMove<dag outs, dag ins, string asm, int size>
    : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asm;
  let Size = size;
  let isCodeGenOnly = 1;
  let hasSideEffects = 0;
}
def MOV_A_IM : MCS51ImagMove<(outs MCS51AReg:$dst), (ins MCS51Imag8:$src),
                             "mov a, $src", 2> {
  bits<1> dst; bits<8> src; bits<16> Inst;
  let dst = 0;
  let Inst{7-0} = 0xE5;
  let Inst{15-8} = src;
}
def MOV_IM_A : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins MCS51AReg:$src),
                             "mov $dst, a", 2> {
  bits<8> dst; bits<1> src; bits<16> Inst;
  let src = 0;
  let Inst{7-0} = 0xF5;
  let Inst{15-8} = dst;
}
def MOV_RN_IM : MCS51ImagMove<(outs MCS51GPR8:$dst), (ins MCS51Imag8:$src),
                              "mov $dst, $src", 2> {
  bits<3> dst; bits<8> src; bits<16> Inst;
  let Inst{7-3} = 0b10101;
  let Inst{2-0} = dst;
  let Inst{15-8} = src;
}
def MOV_IM_RN : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins MCS51GPR8:$src),
                              "mov $dst, $src", 2> {
  bits<8> dst; bits<3> src; bits<16> Inst;
  let Inst{7-3} = 0b10001;
  let Inst{2-0} = src;
  let Inst{15-8} = dst;
}
def MOV_IM_IM : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins MCS51Imag8:$src),
                              "mov $dst, $src", 3> {
  bits<8> dst; bits<8> src; bits<24> Inst;
  let Inst{7-0} = 0x85;
  let Inst{15-8} = src;
  let Inst{23-16} = dst;
}
def MOV_IM_DIRECT : MCS51ImagMove<(outs MCS51Imag8:$dst),
                                  (ins MCS51Direct8:$src),
                                  "mov $dst, $src", 3> {
  let hasSideEffects = 1;
  let mayLoad = 1;
  bits<8> dst; bits<8> src; bits<24> Inst;
  let Inst{7-0} = 0x85;
  let Inst{15-8} = src;
  let Inst{23-16} = dst;
}
def MOV_DIRECT_IM : MCS51ImagMove<(outs), (ins MCS51Direct8:$dst,
                                               MCS51Imag8:$src),
                                  "mov $dst, $src", 3> {
  let hasSideEffects = 1;
  let mayStore = 1;
  bits<8> dst; bits<8> src; bits<24> Inst;
  let Inst{7-0} = 0x85;
  let Inst{15-8} = src;
  let Inst{23-16} = dst;
}
def MOV_IM_R1_IND : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins), "mov $dst, @r1", 2> {
  let hasSideEffects = 1;
  let mayLoad = 1;
  let Uses = [R1];
  bits<8> dst; bits<16> Inst;
  let Inst{7-0} = 0x87;
  let Inst{15-8} = dst;
}
def MOV_R1_IND_IM : MCS51ImagMove<(outs), (ins MCS51Imag8:$src), "mov @r1, $src", 2> {
  let hasSideEffects = 1;
  let mayStore = 1;
  let Uses = [R1];
  bits<8> src; bits<16> Inst;
  let Inst{7-0} = 0xA7;
  let Inst{15-8} = src;
}
class MCS51ImagAcc<bits<8> opcode, string mnemonic, list<Register> defs,
                   list<Register> uses> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Imag8:$src);
  let AsmString = mnemonic # " a, $src";
  let Size = 2;
  let isCodeGenOnly = 1;
  let hasSideEffects = 0;
  let mayLoad = 0;
  let Defs = defs;
  let Uses = uses;
  bits<8> src; bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = src;
}
def ADD_A_IM  : MCS51ImagAcc<0x25, "add",  [A, C], [A]>;
def ADDC_A_IM : MCS51ImagAcc<0x35, "addc", [A, C], [A, C]>;
def SUBB_A_IM : MCS51ImagAcc<0x95, "subb", [A, C], [A, C]>;
def ANL_A_IM  : MCS51ImagAcc<0x55, "anl",  [A],    [A]>;
def ORL_A_IM  : MCS51ImagAcc<0x45, "orl",  [A],    [A]>;
def XRL_A_IM  : MCS51ImagAcc<0x65, "xrl",  [A],    [A]>;
// Spill transfers that leave A untouched: the register's bank-0 address is the
// direct operand of MOV direct,@R1 and MOV @R1,direct.
def MOV_RN_R1_IND : MCS51ImagMove<(outs MCS51GPR8:$dst), (ins), "mov $dst, @r1", 2> {
  let hasSideEffects = 1;
  let mayLoad = 1;
  let Uses = [R1];
  bits<8> dst; bits<16> Inst;
  let Inst{7-0} = 0x87;
  let Inst{15-8} = dst;
}
def MOV_R1_IND_RN : MCS51ImagMove<(outs), (ins MCS51GPR8:$src), "mov @r1, $src", 2> {
  let hasSideEffects = 1;
  let mayStore = 1;
  let Uses = [R1];
  bits<8> src; bits<16> Inst;
  let Inst{7-0} = 0xA7;
  let Inst{15-8} = src;
}
def PUSH_IM : MCS51ImagMove<(outs), (ins MCS51Imag8:$src), "push $src", 2> {
  let hasSideEffects = 1;
  let mayStore = 1;
  let Defs = [SP];
  let Uses = [SP];
  bits<8> src; bits<16> Inst;
  let Inst{7-0} = 0xC0;
  let Inst{15-8} = src;
}
def POP_IM : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins), "pop $dst", 2> {
  let hasSideEffects = 1;
  let mayLoad = 1;
  let Defs = [SP];
  let Uses = [SP];
  bits<8> dst; bits<16> Inst;
  let Inst{7-0} = 0xD0;
  let Inst{15-8} = dst;
}
// A byte of a symbol's address, resolved by the linker.
def MCS51SymLo8 : Operand<i8> { let EncoderMethod = "getSymLo8OpValue"; }
def MCS51SymHi8 : Operand<i8> { let EncoderMethod = "getSymHi8OpValue"; }
def MOV_IM_SYMLO : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins MCS51SymLo8:$sym),
                                 "mov $dst, #lo8($sym)", 3> {
  bits<8> dst; bits<8> sym; bits<24> Inst;
  let isReMaterializable = 1;
  let isAsCheapAsAMove = 1;
  let Inst{7-0} = 0x75;
  let Inst{15-8} = dst;
  let Inst{23-16} = sym;
}
def MOV_IM_SYMHI : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins MCS51SymHi8:$sym),
                                 "mov $dst, #hi8($sym)", 3> {
  bits<8> dst; bits<8> sym; bits<24> Inst;
  let isReMaterializable = 1;
  let isAsCheapAsAMove = 1;
  let Inst{7-0} = 0x75;
  let Inst{15-8} = dst;
  let Inst{23-16} = sym;
}
def MOV_IM_IMM : MCS51ImagMove<(outs MCS51Imag8:$dst), (ins MCS51Imm8:$imm),
                               "mov $dst, #$imm", 3> {
  bits<8> dst; bits<8> imm; bits<24> Inst;
  let isReMaterializable = 1;
  let isAsCheapAsAMove = 1;
  let Inst{7-0} = 0x75;
  let Inst{15-8} = dst;
  let Inst{23-16} = imm;
}

class MCS51MovIndirectDirect<bits<8> opcode, string indirect> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = "mov " # indirect # ", $addr";
  let Size = 2;
  let mayLoad = 1;
  let mayStore = 1;
  bits<8> addr; bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
}
class MCS51MovDirectIndirect<bits<8> opcode, string indirect> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs);
  let InOperandList = (ins MCS51Direct8:$addr);
  let AsmString = "mov $addr, " # indirect;
  let Size = 2;
  let mayLoad = 1;
  let mayStore = 1;
  let Defs = [DPTR, B];
  bits<8> addr; bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = addr;
}
def MOV_R0_IND_DIRECT : MCS51MovIndirectDirect<0xA6, "@r0">;
def MOV_R1_IND_DIRECT : MCS51MovIndirectDirect<0xA7, "@r1">;
def MOV_DIRECT_R0_IND : MCS51MovDirectIndirect<0x86, "@r0">;
def MOV_DIRECT_R1_IND : MCS51MovDirectIndirect<0x87, "@r1">;

class MCS51ExchangeRn : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "xch a, $src";
  let Size = 1;
  let hasSideEffects = 1;
  let Defs = [A];
  let Uses = [A];
  let Constraints = "$dst = $src";
  bits<3> dst;
  bits<8> Inst;
  let Inst{7-3} = 0b11001;
  let Inst{2-0} = dst;
}
def XCH_A_RN : MCS51ExchangeRn;

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

def MOV_A_RN  : MCS51RnInst<0b11101, "mov a,"> { let Defs = [A]; }
def ADD_A_RN  : MCS51RnInst<0b00101, "add a,"> {
  let Defs = [A, C]; let Uses = [A];
}
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
class MCS51ModifyRn<bits<5> opcode, string mnemonic> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = mnemonic # " $src";
  let Size = 1;
  let Constraints = "$dst = $src";
  let hasSideEffects = 1;
  bits<3> dst;
  bits<8> Inst;
  let Inst{7-3} = opcode;
  let Inst{2-0} = dst;
}
def INC_RN : MCS51ModifyRn<0b00001, "inc"> {
  let Pattern = [(set MCS51GPR8:$dst,
                      (add MCS51GPR8:$src, (i8 1)))];
}
def DEC_RN : MCS51ModifyRn<0b00011, "dec"> {
  let Pattern = [(set MCS51GPR8:$dst,
                      (sub MCS51GPR8:$src, (i8 1)))];
}
def MOV_RN_IMM : MCS51RnImmInst<0b01111, "mov"> {
  let Pattern = [(set MCS51GPR8:$rn, MCS51Imm8:$imm)];
}

class MCS51ALUPseudoRR<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst,
                      (node MCS51GPR8:$lhs, MCS51GPR8:$rhs))];
}

class MCS51ALUPseudoRI<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51Imm8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst,
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
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst,
                      (mul MCS51GPR8:$lhs, MCS51GPR8:$rhs))];
}

def MUL8ri : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51Imm8:$rhs);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst,
                      (mul MCS51GPR8:$lhs, MCS51Imm8:$rhs))];
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
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51ShiftAmount:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst,
                      (node MCS51GPR8:$lhs, MCS51ShiftAmount:$amount))];
}

def SHL8ri : MCS51ShiftPseudo<shl>;
def SRL8ri : MCS51ShiftPseudo<srl>;
def SRA8ri : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51ShiftAmount:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst,
                      (MCS51sra8_imm MCS51GPR8:$lhs,
                                     MCS51ShiftAmount:$amount))];
}
def SRA8rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [B];
  let Pattern = [(set MCS51GPR8:$dst,
                      (MCS51sra8 MCS51GPR8:$lhs, MCS51GPR16:$amount))];
}
def SRL8rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [B];
  let Pattern = [(set MCS51GPR8:$dst,
                      (MCS51srl8 MCS51GPR8:$lhs, MCS51GPR16:$amount))];
}
def SHL8rr : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR16:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [B];
  let Pattern = [(set MCS51GPR8:$dst,
                      (MCS51shl8 MCS51GPR8:$lhs, MCS51GPR16:$amount))];
}
class MCS51Shift8RegPseudo<SDNode node> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR8:$lhs, MCS51GPR8:$amount);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Defs = [B];
  let Pattern = [(set MCS51GPR8:$dst,
                      (node MCS51GPR8:$lhs, MCS51GPR8:$amount))];
}
def SRA8rrReg : MCS51Shift8RegPseudo<MCS51sra8_reg>;
def SRL8rrReg : MCS51Shift8RegPseudo<MCS51srl8_reg>;
def SHL8rrReg : MCS51Shift8RegPseudo<MCS51shl8_reg>;

def ZEXT8TO16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst, (zext MCS51GPR8:$src))];
}

def SEXT8TO16 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst, (sext MCS51GPR8:$src))];
}

class MCS51AddExtendedByteImmediatePseudo<SDNode Op> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR8:$src, MCS51Imm16:$imm);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst,
                      (Op MCS51GPR8:$src, MCS51Imm16:$imm))];
}

def ADD_ZEXT8_IMM : MCS51AddExtendedByteImmediatePseudo<MCS51add_zext8_imm>;
def ADD_SEXT8_IMM : MCS51AddExtendedByteImmediatePseudo<MCS51add_sext8_imm>;

class MCS51AddExtendedBytePseudo<SDNode Op> : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR16:$dst);
  let InOperandList = (ins MCS51GPR16:$base, MCS51GPR8:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR16:$dst,
                      (Op MCS51GPR16:$base, MCS51GPR8:$src))];
}

def ADD_ZEXT8_16 : MCS51AddExtendedBytePseudo<MCS51add_zext8_16>;
def ADD_SEXT8_16 : MCS51AddExtendedBytePseudo<MCS51add_sext8_16>;
def SUB_ZEXT8_16 : MCS51AddExtendedBytePseudo<MCS51sub_zext8_16>;
def SUB_SEXT8_16 : MCS51AddExtendedBytePseudo<MCS51sub_sext8_16>;

def TRUNC16TO8 : Instruction {
  let Namespace = "MCS51";
  let OutOperandList = (outs MCS51GPR8:$dst);
  let InOperandList = (ins MCS51GPR16:$src);
  let AsmString = "";
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let usesCustomInserter = 1;
  let hasSideEffects = 1;
  let Pattern = [(set MCS51GPR8:$dst, (trunc MCS51GPR16:$src))];
}
