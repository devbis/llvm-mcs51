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


// Imaginary registers: bytes of direct RAM that the allocator uses as 16-bit
// pairs. They are plain DATA addresses (0x30..0x47); the register name is
// the decimal address so the assembler sees an ordinary direct operand.
class MCS51ImagReg<int address> : Register<!cast<string>(address)> {
  let Namespace = "MCS51";
  let HWEncoding = address;
}
class MCS51ImagPair<int index, MCS51ImagReg lo, MCS51ImagReg hi>
    : RegisterWithSubRegs<"imag" # index, [lo, hi]> {
  let Namespace = "MCS51";
  let HWEncoding = index;
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
}
def IM0 : MCS51ImagReg<48>;
def IM1 : MCS51ImagReg<49>;
def IM2 : MCS51ImagReg<50>;
def IM3 : MCS51ImagReg<51>;
def IM4 : MCS51ImagReg<52>;
def IM5 : MCS51ImagReg<53>;
def IM6 : MCS51ImagReg<54>;
def IM7 : MCS51ImagReg<55>;
def IM8 : MCS51ImagReg<56>;
def IM9 : MCS51ImagReg<57>;
def IM10 : MCS51ImagReg<58>;
def IM11 : MCS51ImagReg<59>;
def IM12 : MCS51ImagReg<60>;
def IM13 : MCS51ImagReg<61>;
def IM14 : MCS51ImagReg<62>;
def IM15 : MCS51ImagReg<63>;
def IM16 : MCS51ImagReg<64>;
def IM17 : MCS51ImagReg<65>;
def IM18 : MCS51ImagReg<66>;
def IM19 : MCS51ImagReg<67>;
def IM20 : MCS51ImagReg<68>;
def IM21 : MCS51ImagReg<69>;
def IM22 : MCS51ImagReg<70>;
def IM23 : MCS51ImagReg<71>;
def IP0 : MCS51ImagPair<0, IM0, IM1>;
def IP1 : MCS51ImagPair<1, IM2, IM3>;
def IP2 : MCS51ImagPair<2, IM4, IM5>;
def IP3 : MCS51ImagPair<3, IM6, IM7>;
def IP4 : MCS51ImagPair<4, IM8, IM9>;
def IP5 : MCS51ImagPair<5, IM10, IM11>;
def IP6 : MCS51ImagPair<6, IM12, IM13>;
def IP7 : MCS51ImagPair<7, IM14, IM15>;
def IP8 : MCS51ImagPair<8, IM16, IM17>;
def IP9 : MCS51ImagPair<9, IM18, IM19>;
def IP10 : MCS51ImagPair<10, IM20, IM21>;
def IP11 : MCS51ImagPair<11, IM22, IM23>;
def MCS51Imag8 : RegisterClass<"MCS51", [i8], 8, (sequence "IM%d", 0, 23)> {
  let isAllocatable = 0;
}

// DPL and DPH are members so that the halves of DPTR count as byte
// subregisters of a word; they are reserved and never allocated to bytes.
def MCS51GPR8 : RegisterClass<"MCS51", [i8], 8,
                              (add R0, R1, R2, R3, R4, R5, R6, R7, DPL, DPH)>;
def MCS51Indirect8 : RegisterClass<"MCS51", [i8], 8, (add R0, R1)>;
def MCS51AReg : RegisterClass<"MCS51", [i8], 8, (add A)>;
def MCS51PTR : RegisterClass<"MCS51", [i16], 8, (add DPTR)>;
def MCS51GPR16 : RegisterClass<"MCS51", [i16], 8,
                               (sequence "IP%d", 0, 11)>;
