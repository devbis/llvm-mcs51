class MCS51Reg<bits<16> encoding, string name> : Register<name> {
  let Namespace = "MCS51";
  let HWEncoding = encoding;
}

let Namespace = "MCS51" in {
  def sub_lo : SubRegIndex<8>;
  def sub_hi : SubRegIndex<8, 8>;
}

def A    : MCS51Reg<0, "a">;
def B    : MCS51Reg<1, "b">;
def C    : MCS51Reg<2, "c">;
def DPL  : MCS51Reg<4, "dpl">;
def DPH  : MCS51Reg<5, "dph">;
// DPTR is the pair DPH:DPL. DPL and DPH are not allocatable; they name the
// halves for copies and for the direct SFR accesses.
def DPTR : RegisterWithSubRegs<"dptr", [DPL, DPH]> {
  let Namespace = "MCS51";
  let HWEncoding = 3;
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
}
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

// Register pairs let 16-bit values wait out DPTR clobbers in the callee-saved
// registers instead of in stack slots.
class MCS51PairReg<bits<16> encoding, string name, list<Register> subregs>
    : RegisterWithSubRegs<name, subregs> {
  let Namespace = "MCS51";
  let HWEncoding = encoding;
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
}
def R2R3 : MCS51PairReg<2, "r2r3", [R2, R3]>;
def R4R5 : MCS51PairReg<4, "r4r5", [R4, R5]>;
def R6R7 : MCS51PairReg<6, "r6r7", [R6, R7]>;

// DPL and DPH are members so that the halves of DPTR count as byte
// subregisters of a word; they are reserved and never allocated to bytes.
def MCS51GPR8 : RegisterClass<"MCS51", [i8], 8,
                              (add R0, R1, R2, R3, R4, R5, R6, R7, DPL, DPH)>;
def MCS51Indirect8 : RegisterClass<"MCS51", [i8], 8, (add R0, R1)>;
def MCS51AReg : RegisterClass<"MCS51", [i8], 8, (add A)>;
def MCS51PTR : RegisterClass<"MCS51", [i16], 8, (add DPTR)>;
def MCS51GPR16 : RegisterClass<"MCS51", [i16], 8, (add DPTR, R2R3, R4R5, R6R7)>;
