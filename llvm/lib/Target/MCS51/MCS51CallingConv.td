def CC_MCS51 : CallingConv<[
  CCIfVarArg<CCIfType<[i8], CCAssignToStack<1, 1>>>,
  CCIfVarArg<CCIfType<[i16], CCAssignToStack<2, 1>>>,
  CCIfType<[i16], CCAssignToReg<[DPTR]>>,
  CCIf<"OrigTy->isIntegerTy(64)",
       CCIfType<[i8], CCAssignToStack<1, 1>>>,
  CCIf<"OrigTy->isIntegerTy(32)",
       CCIfType<[i8], CCAssignToReg<[R4, R5, R6, R7]>>>,
  CCIf<"OrigTy->isFloatTy()",
       CCIfType<[i8], CCAssignToReg<[R4, R5, R6, R7]>>>,
  CCIfType<[i8], CCAssignToReg<[R7, R6, R5, R4]>>,
  CCIfType<[i8], CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 2>>
]>;
def RetCC_MCS51 : CallingConv<[
  CCIf<"OrigTy->isIntegerTy(64)",
       CCIfType<[i8], CCAssignToReg<[R0, R1, R2, R3, R4, R5, R6, R7]>>>,
  CCIf<"OrigTy->isFloatTy()",
       CCIfType<[i8], CCAssignToReg<[R4, R5, R6, R7]>>>,
  CCIfType<[i8], CCAssignToReg<[A]>>,
  CCIfType<[i16], CCAssignToReg<[DPTR]>>
]>;

// R2-R7 are callee-saved. This lets values remain in registers across calls
// instead of spilling each one through the 8051's expensive SP-relative path.
def CSR_MCS51 : CalleeSavedRegs<(add R2, R3, R4, R5, R6, R7)>;
