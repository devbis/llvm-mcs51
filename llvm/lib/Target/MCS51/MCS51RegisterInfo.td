class MCS51Reg<bits<16> encoding, string name> : Register<name> {
  let Namespace = "MCS51";
  let HWEncoding = encoding;
}

def A    : MCS51Reg<0, "a">;
def B    : MCS51Reg<1, "b">;
def C    : MCS51Reg<2, "c">;
def DPTR : MCS51Reg<3, "dptr">;
def DPL  : MCS51Reg<4, "dpl">;
def DPH  : MCS51Reg<5, "dph">;
def SP   : MCS51Reg<6, "sp">;
def PSW  : MCS51Reg<7, "psw">;
def PC   : MCS51Reg<8, "pc">;
// Register-bank encodings are the three-bit n in Rn, unlike the LLVM
// register numbers used for A, B, DPTR and the special registers.
def R0   : MCS51Reg<0, "r0">;
def R1   : MCS51Reg<1, "r1">;
def R2   : MCS51Reg<2, "r2">;
def R3   : MCS51Reg<3, "r3">;
def R4   : MCS51Reg<4, "r4">;
def R5   : MCS51Reg<5, "r5">;
def R6   : MCS51Reg<6, "r6">;
def R7   : MCS51Reg<7, "r7">;

def MCS51GPR8 : RegisterClass<"MCS51", [i8], 8,
                              (add R0, R1, R2, R3, R4, R5, R6, R7)>;
def MCS51AReg : RegisterClass<"MCS51", [i8], 8, (add A)>;
def MCS51PTR : RegisterClass<"MCS51", [i16], 16, (add DPTR)>;
