def CC_MCS51 : CallingConv<[
  CCIfVarArg<CCIfType<[i8], CCAssignToStack<1, 1>>>,
  CCIfVarArg<CCIfType<[i16], CCAssignToStack<2, 1>>>,
  // A 64-bit argument goes on the stack, word by word.
  CCIf<"OrigTy->isIntegerTy(64)", CCIfType<[i16], CCAssignToStack<2, 1>>>,
  // Words travel in imaginary pairs, low word of a wider value first.
  CCIfType<[i16], CCAssignToReg<[IP0, IP1, IP2, IP3]>>,
  CCIfType<[i8], CCAssignToReg<[R7, R6, R5, R4]>>,
  CCIfType<[i8], CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>
]>;
def RetCC_MCS51 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[A]>>,
  CCIfType<[i16], CCAssignToReg<[IP0, IP1, IP2, IP3]>>
]>;

// The IAR 8051 register ABI allocates scalar arguments from left to right.
// Byte arguments use the first free register in R1..R5; words use the
// overlapping R3:R2 and R5:R4 pairs. Stack fallback is diagnosed until the
// XDATA reentrant stack lowering is implemented.
def CC_MCS51_IAR : CallingConv<[
  CCIf<"OrigTy->isIntegerTy(64)", CCIfType<[i16], CCAssignToStack<2, 1>>>,
  CCIfType<[i16], CCAssignToReg<[R2R3, R4R5]>>,
  CCIfType<[i8], CCAssignToReg<[R1, R2, R3, R4, R5]>>,
  CCIfType<[i8], CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>
]>;
def RetCC_MCS51_IAR : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[R1]>>,
  CCIfType<[i16], CCAssignToReg<[R2R3, R4R5]>>
]>;

// R2-R7 and IM8-IM23 are callee-saved. This lets values remain in registers
// across calls instead of spilling each one through the 8051's expensive
// SP-relative path. IM0-IM7 carry arguments and results.
def CSR_MCS51 : CalleeSavedRegs<(add R2, R3, R4, R5, R6, R7,
                                    (sequence "IM%d", 8, 23))>;
// IAR preserves its virtual registers and R6/R7. DPL/DPH are preserved by
// explicit frame saves in MCS51FrameLowering because DPTR is reserved.
def CSR_MCS51_IAR : CalleeSavedRegs<(add R6, R7,
                                        (sequence "IM%d", 0, 23))>;
