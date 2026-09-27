/* Minimal arithmetic runtime for targets without a system libgcc. */
#include <stdint.h>

typedef union {
  uint64_t Value;
  uint8_t Bytes[8];
} Word64Bytes;

typedef union {
  float Float;
  uint32_t Bits;
} Float32Bits;

int __unordsf2(float LHS, float RHS) {
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  uint32_t AExponent = A.Bits & UINT32_C(0x7f800000);
  uint32_t BExponent = B.Bits & UINT32_C(0x7f800000);
  return (AExponent == UINT32_C(0x7f800000) &&
          (A.Bits & UINT32_C(0x007fffff))) ||
         (BExponent == UINT32_C(0x7f800000) &&
          (B.Bits & UINT32_C(0x007fffff)));
}

static uint32_t shift_right_jam32(uint32_t Value, unsigned Count) {
  if (!Count)
    return Value;
  if (Count < 32)
    return (Value >> Count) | ((Value << (32 - Count)) != 0);
  return Value != 0;
}

float __addsf3(float LHS, float RHS) {
  const uint32_t SignMask = UINT32_C(0x80000000);
  const uint32_t ExponentMask = UINT32_C(0x7f800000);
  const uint32_t FractionMask = UINT32_C(0x007fffff);
  const uint32_t HiddenBit = UINT32_C(0x00800000);
  const uint32_t QuietBit = UINT32_C(0x00400000);
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  uint32_t AAbs = A.Bits & ~SignMask;
  uint32_t BAbs = B.Bits & ~SignMask;

  if ((AAbs & ExponentMask) == ExponentMask && (AAbs & FractionMask)) {
    A.Bits |= QuietBit;
    return A.Float;
  }
  if ((BAbs & ExponentMask) == ExponentMask && (BAbs & FractionMask)) {
    B.Bits |= QuietBit;
    return B.Float;
  }
  if (AAbs == ExponentMask || BAbs == ExponentMask) {
    if (AAbs == ExponentMask && BAbs == ExponentMask &&
        ((A.Bits ^ B.Bits) & SignMask)) {
      Float32Bits NaN = {.Bits = UINT32_C(0x7fc00000)};
      return NaN.Float;
    }
    return AAbs == ExponentMask ? A.Float : B.Float;
  }
  if (!AAbs)
    return !BAbs ? (Float32Bits){.Bits = A.Bits & B.Bits}.Float : B.Float;
  if (!BAbs)
    return A.Float;

  if (BAbs > AAbs) {
    Float32Bits Temp = A;
    A = B;
    B = Temp;
  }

  int AExponent = (int)((A.Bits & ExponentMask) >> 23);
  int BExponent = (int)((B.Bits & ExponentMask) >> 23);
  uint32_t ASignificand = A.Bits & FractionMask;
  uint32_t BSignificand = B.Bits & FractionMask;
  if (!AExponent && ASignificand) {
    AExponent = 1;
    while (!(ASignificand & HiddenBit)) {
      ASignificand <<= 1;
      --AExponent;
    }
  }
  if (!BExponent && BSignificand) {
    BExponent = 1;
    while (!(BSignificand & HiddenBit)) {
      BSignificand <<= 1;
      --BExponent;
    }
  }

  uint32_t ResultSign = A.Bits & SignMask;
  int Subtract = (A.Bits ^ B.Bits) & SignMask;
  ASignificand = (ASignificand | HiddenBit) << 3;
  BSignificand = (BSignificand | HiddenBit) << 3;
  BSignificand = shift_right_jam32(BSignificand,
                                   (unsigned)(AExponent - BExponent));

  if (Subtract) {
    ASignificand -= BSignificand;
    if (!ASignificand) {
      Float32Bits Zero = {.Bits = 0};
      return Zero.Float;
    }
    while (ASignificand < (HiddenBit << 3)) {
      ASignificand <<= 1;
      --AExponent;
    }
  } else {
    ASignificand += BSignificand;
    if (ASignificand & (HiddenBit << 4)) {
      ASignificand = shift_right_jam32(ASignificand, 1);
      ++AExponent;
    }
  }

  if (AExponent >= 255) {
    Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
    return Infinity.Float;
  }
  if (AExponent <= 0) {
    ASignificand = shift_right_jam32(ASignificand,
                                     (unsigned)(1 - AExponent));
    AExponent = 0;
  }

  uint32_t RoundGuardSticky = ASignificand & 7;
  Float32Bits Result = {
      .Bits = ResultSign | ((uint32_t)AExponent << 23) |
              ((ASignificand >> 3) & FractionMask)};
  if (RoundGuardSticky > 4 ||
      (RoundGuardSticky == 4 && (Result.Bits & 1)))
    ++Result.Bits;
  return Result.Float;
}

float __subsf3(float LHS, float RHS) {
  Float32Bits Operand = {.Float = RHS};
  Operand.Bits ^= UINT32_C(0x80000000);
  return __addsf3(LHS, Operand.Float);
}

static uint64_t round_shift_right_even64(uint64_t Value, unsigned Count) {
  if (!Count)
    return Value;
  if (Count >= 64)
    return 0;
  uint64_t Quotient = Value >> Count;
  uint64_t Remainder = Value - (Quotient << Count);
  uint64_t Halfway = UINT64_C(1) << (Count - 1);
  return Quotient +
         (Remainder > Halfway ||
          (Remainder == Halfway && (Quotient & 1)));
}

float __mulsf3(float LHS, float RHS) {
  const uint32_t SignMask = UINT32_C(0x80000000);
  const uint32_t ExponentMask = UINT32_C(0x7f800000);
  const uint32_t FractionMask = UINT32_C(0x007fffff);
  const uint32_t HiddenBit = UINT32_C(0x00800000);
  const uint32_t QuietBit = UINT32_C(0x00400000);
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  uint32_t AAbs = A.Bits & ~SignMask;
  uint32_t BAbs = B.Bits & ~SignMask;
  uint32_t ResultSign = (A.Bits ^ B.Bits) & SignMask;

  if ((AAbs & ExponentMask) == ExponentMask && (AAbs & FractionMask)) {
    A.Bits |= QuietBit;
    return A.Float;
  }
  if ((BAbs & ExponentMask) == ExponentMask && (BAbs & FractionMask)) {
    B.Bits |= QuietBit;
    return B.Float;
  }
  if (AAbs == ExponentMask || BAbs == ExponentMask) {
    uint32_t Other = AAbs == ExponentMask ? BAbs : AAbs;
    if (!Other) {
      Float32Bits NaN = {.Bits = UINT32_C(0x7fc00000)};
      return NaN.Float;
    }
    Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
    return Infinity.Float;
  }
  if (!AAbs || !BAbs) {
    Float32Bits Zero = {.Bits = ResultSign};
    return Zero.Float;
  }

  int AExponent = (int)((A.Bits & ExponentMask) >> 23);
  int BExponent = (int)((B.Bits & ExponentMask) >> 23);
  uint32_t ASignificand = A.Bits & FractionMask;
  uint32_t BSignificand = B.Bits & FractionMask;
  if (!AExponent) {
    AExponent = -126;
    while (!(ASignificand & HiddenBit)) {
      ASignificand <<= 1;
      --AExponent;
    }
  } else {
    AExponent -= 127;
    ASignificand |= HiddenBit;
  }
  if (!BExponent) {
    BExponent = -126;
    while (!(BSignificand & HiddenBit)) {
      BSignificand <<= 1;
      --BExponent;
    }
  } else {
    BExponent -= 127;
    BSignificand |= HiddenBit;
  }

  uint64_t Product = (uint64_t)ASignificand * BSignificand;
  int HasTopBit = (Product & (UINT64_C(1) << 47)) != 0;
  int ResultExponent = AExponent + BExponent + HasTopBit;
  if (ResultExponent > 127) {
    Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
    return Infinity.Float;
  }

  uint64_t RoundedSignificand;
  if (ResultExponent >= -126) {
    RoundedSignificand = round_shift_right_even64(Product,
                                                 HasTopBit ? 24 : 23);
    if (RoundedSignificand == (UINT64_C(1) << 24)) {
      RoundedSignificand >>= 1;
      ++ResultExponent;
    }
    if (ResultExponent > 127) {
      Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
      return Infinity.Float;
    }
    Float32Bits Result = {
        .Bits = ResultSign | ((uint32_t)(ResultExponent + 127) << 23) |
                ((uint32_t)RoundedSignificand & FractionMask)};
    return Result.Float;
  }

  int SubnormalShift = -(AExponent + BExponent + 103);
  if (SubnormalShift >= 49)
    RoundedSignificand = 0;
  else
    RoundedSignificand =
        round_shift_right_even64(Product, (unsigned)SubnormalShift);
  Float32Bits Result = {.Bits = ResultSign | (uint32_t)RoundedSignificand};
  return Result.Float;
}

float __divsf3(float LHS, float RHS) {
  const uint32_t SignMask = UINT32_C(0x80000000);
  const uint32_t ExponentMask = UINT32_C(0x7f800000);
  const uint32_t FractionMask = UINT32_C(0x007fffff);
  const uint32_t HiddenBit = UINT32_C(0x00800000);
  const uint32_t QuietBit = UINT32_C(0x00400000);
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  uint32_t AAbs = A.Bits & ~SignMask;
  uint32_t BAbs = B.Bits & ~SignMask;
  uint32_t ResultSign = (A.Bits ^ B.Bits) & SignMask;

  if ((AAbs & ExponentMask) == ExponentMask && (AAbs & FractionMask)) {
    A.Bits |= QuietBit;
    return A.Float;
  }
  if ((BAbs & ExponentMask) == ExponentMask && (BAbs & FractionMask)) {
    B.Bits |= QuietBit;
    return B.Float;
  }
  if (AAbs == ExponentMask || BAbs == ExponentMask) {
    if (AAbs == ExponentMask && BAbs == ExponentMask) {
      Float32Bits NaN = {.Bits = UINT32_C(0x7fc00000)};
      return NaN.Float;
    }
    if (BAbs == ExponentMask) {
      Float32Bits Zero = {.Bits = ResultSign};
      return Zero.Float;
    }
    Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
    return Infinity.Float;
  }
  if (!BAbs) {
    if (!AAbs) {
      Float32Bits NaN = {.Bits = UINT32_C(0x7fc00000)};
      return NaN.Float;
    }
    Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
    return Infinity.Float;
  }
  if (!AAbs) {
    Float32Bits Zero = {.Bits = ResultSign};
    return Zero.Float;
  }

  int AExponent = (int)((A.Bits & ExponentMask) >> 23);
  int BExponent = (int)((B.Bits & ExponentMask) >> 23);
  uint32_t ASignificand = A.Bits & FractionMask;
  uint32_t BSignificand = B.Bits & FractionMask;
  if (!AExponent) {
    AExponent = -126;
    while (!(ASignificand & HiddenBit)) {
      ASignificand <<= 1;
      --AExponent;
    }
  } else {
    AExponent -= 127;
    ASignificand |= HiddenBit;
  }
  if (!BExponent) {
    BExponent = -126;
    while (!(BSignificand & HiddenBit)) {
      BSignificand <<= 1;
      --BExponent;
    }
  } else {
    BExponent -= 127;
    BSignificand |= HiddenBit;
  }

  int ResultExponent = AExponent - BExponent;
  if (ASignificand < BSignificand) {
    ASignificand <<= 1;
    --ResultExponent;
  }
  uint32_t Remainder = ASignificand;
  uint32_t ExtendedSignificand = 0;
  for (unsigned I = 0; I != 27; ++I) {
    ExtendedSignificand <<= 1;
    if (Remainder >= BSignificand) {
      Remainder -= BSignificand;
      ExtendedSignificand |= 1;
    }
    Remainder <<= 1;
  }
  if (Remainder)
    ExtendedSignificand |= 1;

  if (ResultExponent > 127) {
    Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
    return Infinity.Float;
  }
  if (ResultExponent < -126)
    ExtendedSignificand =
        shift_right_jam32(ExtendedSignificand,
                          (unsigned)(-126 - ResultExponent));

  uint32_t RoundGuardSticky = ExtendedSignificand & 7;
  uint32_t RoundedSignificand = ExtendedSignificand >> 3;
  if (RoundGuardSticky > 4 ||
      (RoundGuardSticky == 4 && (RoundedSignificand & 1)))
    ++RoundedSignificand;

  uint32_t ResultExponentField;
  if (ResultExponent < -126) {
    if (RoundedSignificand >= HiddenBit) {
      ResultExponentField = 1;
      RoundedSignificand = 0;
    } else {
      ResultExponentField = 0;
    }
  } else {
    if (RoundedSignificand == (HiddenBit << 1)) {
      RoundedSignificand >>= 1;
      ++ResultExponent;
    }
    if (ResultExponent > 127) {
      Float32Bits Infinity = {.Bits = ExponentMask | ResultSign};
      return Infinity.Float;
    }
    ResultExponentField = (uint32_t)(ResultExponent + 127);
  }

  Float32Bits Result = {.Bits = ResultSign | (ResultExponentField << 23) |
                                (RoundedSignificand & FractionMask)};
  return Result.Float;
}

uint64_t __ashldi3(uint64_t Value, int Count) {
  Word64Bytes Result = {.Value = Value};
  if (Count >= 64)
    return 0;
  for (int I = 0; I < Count; ++I) {
    uint8_t Carry = 0;
    for (unsigned Byte = 0; Byte != 8; ++Byte) {
      uint8_t NextCarry = Result.Bytes[Byte] >> 7;
      Result.Bytes[Byte] = (uint8_t)((Result.Bytes[Byte] << 1) | Carry);
      Carry = NextCarry;
    }
  }
  return Result.Value;
}

uint64_t __lshrdi3(uint64_t Value, int Count) {
  Word64Bytes Result = {.Value = Value};
  if (Count >= 64)
    return 0;
  for (int I = 0; I < Count; ++I) {
    uint8_t Carry = 0;
    for (int Byte = 7; Byte >= 0; --Byte) {
      uint8_t NextCarry = Result.Bytes[Byte] & 1;
      Result.Bytes[Byte] = (uint8_t)((Result.Bytes[Byte] >> 1) | (Carry << 7));
      Carry = NextCarry;
    }
  }
  return Result.Value;
}

int64_t __ashrdi3(int64_t Value, int Count) {
  Word64Bytes Result = {.Value = (uint64_t)Value};
  if (Count >= 64)
    Count = 64;
  for (int I = 0; I < Count; ++I) {
    uint8_t Carry = Result.Bytes[7] & 0x80;
    for (int Byte = 7; Byte >= 0; --Byte) {
      uint8_t NextCarry = (uint8_t)((Result.Bytes[Byte] & 1) << 7);
      Result.Bytes[Byte] = (uint8_t)((Result.Bytes[Byte] >> 1) | Carry);
      Carry = NextCarry;
    }
  }
  return (int64_t)Result.Value;
}

static uint64_t udivmod64(uint64_t Numerator, uint64_t Denominator,
                          uint64_t *Remainder) {
  uint64_t Quotient = 0;
  uint64_t Rest = 0;

  for (unsigned I = 0; I != 64; ++I) {
    unsigned RemainderCarry = Rest >> 63;
    unsigned InputBit = Numerator >> 63;
    Numerator <<= 1;
    Rest = (Rest << 1) | InputBit;
    Word64Bytes RestBytes = {.Value = Rest};
    Word64Bytes DenominatorBytes = {.Value = Denominator};
    unsigned GreaterOrEqual = 1;
    for (int Byte = 7; Byte >= 0; --Byte) {
      unsigned Difference = (unsigned)RestBytes.Bytes[Byte] -
                           (unsigned)DenominatorBytes.Bytes[Byte];
      if (Difference != 0) {
        GreaterOrEqual = Difference < 256;
        break;
      }
    }
    if (RemainderCarry || GreaterOrEqual) {
      Rest -= Denominator;
      Numerator |= 1;
    }
  }

  *Remainder = Rest;
  return Numerator;
}

uint64_t __udivdi3(uint64_t Numerator, uint64_t Denominator) {
  uint64_t Remainder;
  return udivmod64(Numerator, Denominator, &Remainder);
}

uint64_t __umoddi3(uint64_t Numerator, uint64_t Denominator) {
  uint64_t Remainder;
  (void)udivmod64(Numerator, Denominator, &Remainder);
  return Remainder;
}

int64_t __divdi3(int64_t Numerator, int64_t Denominator) {
  uint64_t UnsignedNumerator = (uint64_t)Numerator;
  uint64_t UnsignedDenominator = (uint64_t)Denominator;
  unsigned NumeratorNegative = UnsignedNumerator >> 63;
  unsigned DenominatorNegative = UnsignedDenominator >> 63;

  if (NumeratorNegative)
    UnsignedNumerator = 0 - UnsignedNumerator;
  if (DenominatorNegative)
    UnsignedDenominator = 0 - UnsignedDenominator;
  uint64_t Remainder;
  uint64_t Quotient = udivmod64(UnsignedNumerator, UnsignedDenominator,
                                &Remainder);
  if (NumeratorNegative ^ DenominatorNegative)
    Quotient = 0 - Quotient;
  return (int64_t)Quotient;
}

int64_t __moddi3(int64_t Numerator, int64_t Denominator) {
  uint64_t UnsignedNumerator = (uint64_t)Numerator;
  uint64_t UnsignedDenominator = (uint64_t)Denominator;
  unsigned NumeratorNegative = UnsignedNumerator >> 63;
  unsigned DenominatorNegative = UnsignedDenominator >> 63;
  if (NumeratorNegative)
    UnsignedNumerator = 0 - UnsignedNumerator;
  if (DenominatorNegative)
    UnsignedDenominator = 0 - UnsignedDenominator;
  uint64_t Remainder;
  (void)udivmod64(UnsignedNumerator, UnsignedDenominator, &Remainder);
  if (NumeratorNegative)
    Remainder = 0 - Remainder;
  return (int64_t)Remainder;
}

static __attribute__((noinline)) uint32_t
udivmod16(uint16_t Numerator, uint16_t Denominator) {
  uint16_t Quotient = 0;
  uint16_t Rest = 0;

  for (unsigned I = 0; I != 16; ++I) {
    unsigned RemainderCarry = Rest >> 15;
    unsigned InputBit = Numerator >> 15;
    Numerator <<= 1;
    Rest = (uint16_t)((Rest << 1) | InputBit);
    if (RemainderCarry || Rest >= Denominator) {
      Rest -= Denominator;
      Numerator |= 1;
    }
  }

  return ((uint32_t)Rest << 16) | Numerator;
}

uint16_t __udivhi3(uint16_t Numerator, uint16_t Denominator) {
  return (uint16_t)udivmod16(Numerator, Denominator);
}

uint16_t __umodhi3(uint16_t Numerator, uint16_t Denominator) {
  return (uint16_t)(udivmod16(Numerator, Denominator) >> 16);
}

int16_t __divhi3(int16_t Numerator, int16_t Denominator) {
  uint16_t UnsignedNumerator = (uint16_t)Numerator;
  uint16_t UnsignedDenominator = (uint16_t)Denominator;
  unsigned Negative = (Numerator < 0) ^ (Denominator < 0);

  if (Numerator < 0)
    UnsignedNumerator = (uint16_t)(0 - UnsignedNumerator);
  if (Denominator < 0)
    UnsignedDenominator = (uint16_t)(0 - UnsignedDenominator);
  uint16_t Quotient = (uint16_t)udivmod16(UnsignedNumerator,
                                          UnsignedDenominator);
  return Negative ? (int16_t)(0 - Quotient) : (int16_t)Quotient;
}

int16_t __modhi3(int16_t Numerator, int16_t Denominator) {
  uint16_t UnsignedNumerator = (uint16_t)Numerator;
  uint16_t UnsignedDenominator = (uint16_t)Denominator;

  if (Numerator < 0)
    UnsignedNumerator = (uint16_t)(0 - UnsignedNumerator);
  if (Denominator < 0)
    UnsignedDenominator = (uint16_t)(0 - UnsignedDenominator);
  uint16_t Remainder = (uint16_t)(udivmod16(UnsignedNumerator,
                                             UnsignedDenominator) >> 16);
  return Numerator < 0 ? (int16_t)(0 - Remainder) : (int16_t)Remainder;
}

static uint32_t
udivmod32(uint32_t Numerator, uint32_t Denominator, uint32_t *Remainder) {
  uint32_t Quotient = 0;
  uint32_t Rest = 0;
  uint16_t DenominatorHigh = (uint16_t)(Denominator >> 16);
  uint16_t DenominatorLow = (uint16_t)Denominator;

  for (unsigned I = 0; I != 32; ++I) {
    unsigned RemainderCarry = Rest >> 31;
    unsigned InputBit = Numerator >> 31;
    Numerator <<= 1;
    Rest = (Rest << 1) | InputBit;
    uint16_t RestHigh = (uint16_t)(Rest >> 16);
    uint16_t RestLow = (uint16_t)Rest;
    if (RemainderCarry || RestHigh > DenominatorHigh ||
        (RestHigh == DenominatorHigh && RestLow >= DenominatorLow)) {
      Rest -= Denominator;
      Numerator |= 1;
    }
  }

  *Remainder = Rest;
  return Numerator;
}

uint32_t __udivsi3(uint32_t Numerator, uint32_t Denominator) {
  uint32_t Remainder;
  return udivmod32(Numerator, Denominator, &Remainder);
}

uint32_t __umodsi3(uint32_t Numerator, uint32_t Denominator) {
  uint32_t Remainder;
  (void)udivmod32(Numerator, Denominator, &Remainder);
  return Remainder;
}

int32_t __divsi3(int32_t Numerator, int32_t Denominator) {
  uint32_t UnsignedNumerator = (uint32_t)Numerator;
  uint32_t UnsignedDenominator = (uint32_t)Denominator;
  unsigned NumeratorNegative = UnsignedNumerator >> 31;
  unsigned DenominatorNegative = UnsignedDenominator >> 31;
  if (NumeratorNegative)
    UnsignedNumerator = 0 - UnsignedNumerator;
  if (DenominatorNegative)
    UnsignedDenominator = 0 - UnsignedDenominator;
  uint32_t Remainder;
  uint32_t Quotient = udivmod32(UnsignedNumerator, UnsignedDenominator,
                                &Remainder);
  if (NumeratorNegative ^ DenominatorNegative)
    Quotient = 0 - Quotient;
  return (int32_t)Quotient;
}

int32_t __modsi3(int32_t Numerator, int32_t Denominator) {
  uint32_t UnsignedNumerator = (uint32_t)Numerator;
  uint32_t UnsignedDenominator = (uint32_t)Denominator;
  unsigned NumeratorNegative = UnsignedNumerator >> 31;
  unsigned DenominatorNegative = UnsignedDenominator >> 31;
  if (NumeratorNegative)
    UnsignedNumerator = 0 - UnsignedNumerator;
  if (DenominatorNegative)
    UnsignedDenominator = 0 - UnsignedDenominator;
  uint32_t Remainder;
  (void)udivmod32(UnsignedNumerator, UnsignedDenominator, &Remainder);
  if (NumeratorNegative)
    Remainder = 0 - Remainder;
  return (int32_t)Remainder;
}
