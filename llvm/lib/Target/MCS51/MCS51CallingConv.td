def CC_MCS51 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[R7, R6, R5, R4]>>,
  CCIfType<[i8], CCAssignToStack<1, 1>>
]>;
def RetCC_MCS51 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[A]>>
]>;
