def CC_MCS51 : CallingConv<[
  CCIfVarArg<CCIfType<[i8], CCAssignToStack<1, 1>>>,
  CCIfVarArg<CCIfType<[i16], CCAssignToStack<2, 1>>>,
  // A 64-bit argument goes on the stack, word by word.
  CCIf<"OrigTy->isIntegerTy(64)", CCIfType<[i16], CCAssignToStack<2, 1>>>,
  // Words travel in imaginary pairs, low word of a wider value first.
  CCIfType<[i16], CCAssignToReg<[IP0, IP1, IP2, IP3]>>,
  CCIfType<[i8], CCAssignToReg<[R7, R6, R5, R4]>>,
  // Experimental -mcs51-wide-args: continue in caller-saved imaginary
  // registers before using the hardware stack.
  CCIf<"llvm::mcs51WideArgs()",
       CCIfType<[i16], CCAssignToReg<[IP4, IP5, IP6, IP7]>>>,
  CCIf<"llvm::mcs51WideArgs()",
       CCIfType<[i8], CCAssignToReg<[IM16, IM17, IM18, IM19,
                                     IM20, IM21, IM22, IM23]>>>,
  CCIfType<[i8], CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 1>>
]>;
def RetCC_MCS51 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[A]>>,
  CCIfType<[i16], CCAssignToReg<[IP0, IP1, IP2, IP3]>>
]>;

// R2-R7 and IM8-IM23 are callee-saved. This lets values remain in registers
// across calls instead of spilling each one through the 8051's expensive
// SP-relative path. IM0-IM7 carry arguments and results.
def CSR_MCS51 : CalleeSavedRegs<(add R2, R3, R4, R5, R6, R7,
                                    (sequence "IM%d", 8, 23))>;

// Reduced callee-saved sets selected by -mcs51-csr (default unchanged).
def CSR_MCS51_R : CalleeSavedRegs<(add R2, R3, R4, R5, R6, R7)>;
def CSR_MCS51_None : CalleeSavedRegs<(add)>;
