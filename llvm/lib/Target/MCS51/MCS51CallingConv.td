def CC_MCS51 : CallingConv<[
  CCIfType<[i16], CCAssignToReg<[DPTR]>>,
  CCIf<"OrigTy->isIntegerTy(32)",
       CCIfType<[i8], CCAssignToReg<[R4, R5, R6, R7]>>>,
  CCIfType<[i8], CCAssignToReg<[R7, R6, R5, R4]>>,
  CCIfType<[i8], CCAssignToStack<1, 1>>,
  CCIfType<[i16], CCAssignToStack<2, 2>>
]>;
def RetCC_MCS51 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[A]>>,
  CCIfType<[i16], CCAssignToReg<[DPTR]>>
]>;
