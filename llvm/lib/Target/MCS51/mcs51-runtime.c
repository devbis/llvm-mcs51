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

typedef volatile uint8_t __attribute__((address_space(2))) *MCS51IDataPtr;
typedef volatile uint8_t __attribute__((address_space(3))) *MCS51PDataPtr;
typedef volatile uint8_t __attribute__((address_space(4))) *MCS51XDataPtr;
typedef const volatile uint8_t __attribute__((address_space(5))) *MCS51CodePtr;

int32_t __unordsf2(float LHS, float RHS) {
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  uint32_t AExponent = A.Bits & UINT32_C(0x7f800000);
  uint32_t BExponent = B.Bits & UINT32_C(0x7f800000);
  return (AExponent == UINT32_C(0x7f800000) &&
          (A.Bits & UINT32_C(0x007fffff))) ||
         (BExponent == UINT32_C(0x7f800000) &&
          (B.Bits & UINT32_C(0x007fffff)));
}

static __attribute__((noinline)) int8_t compare_float32_ordered(uint32_t A,
                                                                uint32_t B) {
  const uint32_t SignMask = UINT32_C(0x80000000);
  uint32_t AKey = (A ^ (SignMask | (0 - (A >> 31)))) + (A >> 31);
  uint32_t BKey = (B ^ (SignMask | (0 - (B >> 31)))) + (B >> 31);
  return (int8_t)(AKey > BKey) - (int8_t)(AKey < BKey);
}

int32_t __lesf2(float LHS, float RHS) {
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  if (__unordsf2(LHS, RHS))
    return 1;
  return compare_float32_ordered(A.Bits, B.Bits);
}

int32_t __ltsf2(float LHS, float RHS) { return __lesf2(LHS, RHS); }
int32_t __eqsf2(float LHS, float RHS) { return __lesf2(LHS, RHS); }
int32_t __nesf2(float LHS, float RHS) { return __lesf2(LHS, RHS); }

int32_t __gesf2(float LHS, float RHS) {
  Float32Bits A = {.Float = LHS};
  Float32Bits B = {.Float = RHS};
  if (__unordsf2(LHS, RHS))
    return -1;
  return compare_float32_ordered(A.Bits, B.Bits);
}

int32_t __gtsf2(float LHS, float RHS) { return __gesf2(LHS, RHS); }

int32_t __fixsfsi(float Value) {
  const uint32_t SignificandMask = UINT32_C(0x007fffff);
  const uint32_t HiddenBit = UINT32_C(0x00800000);
  Float32Bits Bits = {.Float = Value};
  uint32_t Sign = Bits.Bits >> 31;
  uint32_t SignMask = 0 - Sign;
  uint32_t RawExponent = (Bits.Bits >> 23) & 0xff;
  uint32_t Exponent = RawExponent - 127;
  uint32_t Underflow = Exponent >> 31;
  uint32_t Overflow = ((RawExponent - 158) >> 31) ^ 1;
  uint32_t Valid = (Underflow ^ 1) & (Overflow ^ 1);
  uint32_t Significand = (Bits.Bits & SignificandMask) | HiddenBit;
  uint32_t ShiftLeft = ((Exponent - 23) >> 31) ^ 1;
  uint32_t ShiftMask = 0 - ShiftLeft;
  uint32_t RightShift = (23 - Exponent) & 31;
  uint32_t LeftShift = (Exponent - 23) & 31;
  uint32_t RightMagnitude = Significand >> RightShift;
  uint32_t LeftMagnitude = Significand << LeftShift;
  uint32_t Magnitude = (RightMagnitude & ~ShiftMask) |
                       (LeftMagnitude & ShiftMask);
  uint32_t SignedMagnitude = (Magnitude ^ SignMask) + Sign;
  uint32_t Saturated = UINT32_C(0x7fffffff) ^ SignMask;
  uint32_t ValidMask = 0 - Valid;
  uint32_t OverflowMask = 0 - Overflow;
  return (int32_t)((SignedMagnitude & ValidMask) |
                   (Saturated & OverflowMask));
}

uint32_t __fixunssfsi(float Value) {
  const uint32_t SignificandMask = UINT32_C(0x007fffff);
  const uint32_t HiddenBit = UINT32_C(0x00800000);
  Float32Bits Bits = {.Float = Value};
  uint32_t Sign = Bits.Bits >> 31;
  uint32_t RawExponent = (Bits.Bits >> 23) & 0xff;
  uint32_t Exponent = RawExponent - 127;
  uint32_t Underflow = Exponent >> 31;
  uint32_t Overflow = ((RawExponent - 159) >> 31) ^ 1;
  uint32_t Valid = (Sign ^ 1) & (Underflow ^ 1) & (Overflow ^ 1);
  uint32_t Saturate = (Sign ^ 1) & Overflow;
  uint32_t Significand = (Bits.Bits & SignificandMask) | HiddenBit;
  uint32_t ShiftLeft = ((Exponent - 23) >> 31) ^ 1;
  uint32_t ShiftMask = 0 - ShiftLeft;
  uint32_t RightShift = (23 - Exponent) & 31;
  uint32_t LeftShift = (Exponent - 23) & 31;
  uint32_t RightMagnitude = Significand >> RightShift;
  uint32_t LeftMagnitude = Significand << LeftShift;
  uint32_t Magnitude = (RightMagnitude & ~ShiftMask) |
                       (LeftMagnitude & ShiftMask);
  uint32_t ValidMask = 0 - Valid;
  uint32_t SaturateMask = 0 - Saturate;
  return (Magnitude & ValidMask) | (UINT32_MAX & SaturateMask);
}

static uint32_t uint64_to_float32_bits(uint64_t Magnitude, uint32_t Sign) {
  if (!Magnitude)
    return 0;

  int Exponent = 23;
  uint64_t Probe = Magnitude;
  while (Probe >= UINT64_C(0x01000000)) {
    Probe >>= 1;
    ++Exponent;
  }
  while (Probe < UINT64_C(0x00800000)) {
    Probe <<= 1;
    --Exponent;
  }

  uint64_t Significand;
  if (Exponent > 23) {
    unsigned Shift = Exponent - 23;
    Significand = Magnitude >> Shift;
    uint64_t Discarded = Magnitude & ((UINT64_C(1) << Shift) - 1);
    uint64_t Halfway = UINT64_C(1) << (Shift - 1);
    if (Discarded > Halfway ||
        (Discarded == Halfway && (Significand & 1)))
      ++Significand;
  } else {
    Significand = Magnitude << (23 - Exponent);
  }

  if (Significand == UINT64_C(0x01000000)) {
    Significand >>= 1;
    ++Exponent;
  }

  return (Sign << 31) | ((uint32_t)(Exponent + 127) << 23) |
         ((uint32_t)Significand & UINT32_C(0x007fffff));
}

float __floatsisf(int32_t Value) {
  uint32_t Bits = (uint32_t)Value;
  uint32_t Sign = Bits >> 31;
  uint32_t SignMask = 0 - Sign;
  uint32_t Magnitude = (Bits ^ SignMask) + Sign;
  Float32Bits Result = {.Bits = uint64_to_float32_bits(Magnitude, Sign)};
  return Result.Float;
}

float __floatunsisf(uint32_t Value) {
  Float32Bits Result = {.Bits = uint64_to_float32_bits(Value, 0)};
  return Result.Float;
}

int64_t __fixsfdi(float Value) {
  const uint64_t SignificandMask = UINT64_C(0x007fffff);
  const uint64_t HiddenBit = UINT64_C(0x00800000);
  Float32Bits Bits = {.Float = Value};
  uint32_t Sign = Bits.Bits >> 31;
  uint64_t SignMask = 0 - (uint64_t)Sign;
  uint32_t RawExponent = (Bits.Bits >> 23) & 0xff;
  uint32_t Exponent = RawExponent - 127;
  uint32_t Underflow = Exponent >> 31;
  uint32_t Overflow = ((RawExponent - 190) >> 31) ^ 1;
  uint32_t Valid = (Underflow ^ 1) & (Overflow ^ 1);
  uint64_t Significand = (Bits.Bits & SignificandMask) | HiddenBit;
  uint32_t ShiftLeft = ((Exponent - 23) >> 31) ^ 1;
  uint64_t ShiftMask = 0 - (uint64_t)ShiftLeft;
  uint32_t RightShift = (23 - Exponent) & 63;
  uint32_t LeftShift = (Exponent - 23) & 63;
  uint64_t RightMagnitude = Significand >> RightShift;
  uint64_t LeftMagnitude = Significand << LeftShift;
  uint64_t Magnitude = (RightMagnitude & ~ShiftMask) |
                       (LeftMagnitude & ShiftMask);
  uint64_t SignedMagnitude = (Magnitude ^ SignMask) + Sign;
  uint64_t Saturated = UINT64_C(0x7fffffffffffffff) ^ SignMask;
  uint64_t ValidMask = 0 - (uint64_t)Valid;
  uint64_t OverflowMask = 0 - (uint64_t)Overflow;
  return (int64_t)((SignedMagnitude & ValidMask) |
                   (Saturated & OverflowMask));
}

uint64_t __fixunssfdi(float Value) {
  const uint64_t SignificandMask = UINT64_C(0x007fffff);
  const uint64_t HiddenBit = UINT64_C(0x00800000);
  Float32Bits Bits = {.Float = Value};
  uint32_t Sign = Bits.Bits >> 31;
  uint32_t RawExponent = (Bits.Bits >> 23) & 0xff;
  uint32_t Exponent = RawExponent - 127;
  uint32_t Underflow = Exponent >> 31;
  uint32_t Overflow = ((RawExponent - 191) >> 31) ^ 1;
  uint32_t Valid = (Sign ^ 1) & (Underflow ^ 1) & (Overflow ^ 1);
  uint32_t Saturate = (Sign ^ 1) & Overflow;
  uint64_t Significand = (Bits.Bits & SignificandMask) | HiddenBit;
  uint32_t ShiftLeft = ((Exponent - 23) >> 31) ^ 1;
  uint64_t ShiftMask = 0 - (uint64_t)ShiftLeft;
  uint32_t RightShift = (23 - Exponent) & 63;
  uint32_t LeftShift = (Exponent - 23) & 63;
  uint64_t RightMagnitude = Significand >> RightShift;
  uint64_t LeftMagnitude = Significand << LeftShift;
  uint64_t Magnitude = (RightMagnitude & ~ShiftMask) |
                       (LeftMagnitude & ShiftMask);
  uint64_t ValidMask = 0 - (uint64_t)Valid;
  uint64_t SaturateMask = 0 - (uint64_t)Saturate;
  return (Magnitude & ValidMask) | (UINT64_MAX & SaturateMask);
}

float __floatdisf(int64_t Value) {
  uint64_t Bits = (uint64_t)Value;
  uint32_t Sign = Bits >> 63;
  uint64_t SignMask = 0 - (uint64_t)Sign;
  uint64_t Magnitude = (Bits ^ SignMask) + Sign;
  Float32Bits Result = {.Bits = uint64_to_float32_bits(Magnitude, Sign)};
  return Result.Float;
}

float __floatundisf(uint64_t Value) {
  Float32Bits Result = {.Bits = uint64_to_float32_bits(Value, 0)};
  return Result.Float;
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

/* The MCS-51 backend has no native 64-bit multiply. Multiply bytes and carry
 * row by row so signed and unsigned callers share modulo-2^64 semantics. */
uint64_t __muldi3(uint64_t LHS, uint64_t RHS) {
  Word64Bytes Left = {.Value = LHS};
  Word64Bytes Right = {.Value = RHS};
  Word64Bytes Product = {.Value = 0};
  for (unsigned I = 0; I != 8; ++I) {
    uint16_t Carry = 0;
    for (unsigned J = 0; J != 8 - I; ++J) {
      uint16_t Partial = (uint16_t)Left.Bytes[I] * Right.Bytes[J];
      uint16_t Sum = Partial + Product.Bytes[I + J] + Carry;
      Product.Bytes[I + J] = (uint8_t)Sum;
      Carry = Sum >> 8;
    }
  }
  return Product.Value;
}

uint64_t __ashldi3(uint64_t Value, int Count) {
  Word64Bytes Result = {.Value = Value};
  if (Count >= 64)
    return 0;
  if (Count <= 0)
    return Result.Value;

  unsigned BitShift = (unsigned)Count & 7;
  if (Count & 32) {
    Result.Bytes[7] = Result.Bytes[3];
    Result.Bytes[6] = Result.Bytes[2];
    Result.Bytes[5] = Result.Bytes[1];
    Result.Bytes[4] = Result.Bytes[0];
    Result.Bytes[3] = Result.Bytes[2] = Result.Bytes[1] = Result.Bytes[0] = 0;
  }
  if (Count & 16) {
    Result.Bytes[7] = Result.Bytes[5];
    Result.Bytes[6] = Result.Bytes[4];
    Result.Bytes[5] = Result.Bytes[3];
    Result.Bytes[4] = Result.Bytes[2];
    Result.Bytes[3] = Result.Bytes[1];
    Result.Bytes[2] = Result.Bytes[0];
    Result.Bytes[1] = Result.Bytes[0] = 0;
  }
  if (Count & 8) {
    Result.Bytes[7] = Result.Bytes[6];
    Result.Bytes[6] = Result.Bytes[5];
    Result.Bytes[5] = Result.Bytes[4];
    Result.Bytes[4] = Result.Bytes[3];
    Result.Bytes[3] = Result.Bytes[2];
    Result.Bytes[2] = Result.Bytes[1];
    Result.Bytes[1] = Result.Bytes[0];
    Result.Bytes[0] = 0;
  }
  for (unsigned I = 0; I != BitShift; ++I) {
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
  if (Count <= 0)
    return Result.Value;

  unsigned BitShift = (unsigned)Count & 7;
  if (Count & 32) {
    Result.Bytes[0] = Result.Bytes[4];
    Result.Bytes[1] = Result.Bytes[5];
    Result.Bytes[2] = Result.Bytes[6];
    Result.Bytes[3] = Result.Bytes[7];
    Result.Bytes[4] = Result.Bytes[5] = Result.Bytes[6] = Result.Bytes[7] = 0;
  }
  if (Count & 16) {
    Result.Bytes[0] = Result.Bytes[2];
    Result.Bytes[1] = Result.Bytes[3];
    Result.Bytes[2] = Result.Bytes[4];
    Result.Bytes[3] = Result.Bytes[5];
    Result.Bytes[4] = Result.Bytes[6];
    Result.Bytes[5] = Result.Bytes[7];
    Result.Bytes[6] = Result.Bytes[7] = 0;
  }
  if (Count & 8) {
    Result.Bytes[0] = Result.Bytes[1];
    Result.Bytes[1] = Result.Bytes[2];
    Result.Bytes[2] = Result.Bytes[3];
    Result.Bytes[3] = Result.Bytes[4];
    Result.Bytes[4] = Result.Bytes[5];
    Result.Bytes[5] = Result.Bytes[6];
    Result.Bytes[6] = Result.Bytes[7];
    Result.Bytes[7] = 0;
  }
  for (unsigned I = 0; I != BitShift; ++I) {
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
    return Value < 0 ? -1 : 0;
  if (Count <= 0)
    return (int64_t)Result.Value;

  unsigned BitShift = (unsigned)Count & 7;
  uint8_t Sign = Result.Bytes[7] & 0x80 ? 0xff : 0;
  if (Count & 32) {
    Result.Bytes[0] = Result.Bytes[4];
    Result.Bytes[1] = Result.Bytes[5];
    Result.Bytes[2] = Result.Bytes[6];
    Result.Bytes[3] = Result.Bytes[7];
    Result.Bytes[4] = Result.Bytes[5] = Result.Bytes[6] = Result.Bytes[7] = Sign;
  }
  if (Count & 16) {
    Result.Bytes[0] = Result.Bytes[2];
    Result.Bytes[1] = Result.Bytes[3];
    Result.Bytes[2] = Result.Bytes[4];
    Result.Bytes[3] = Result.Bytes[5];
    Result.Bytes[4] = Result.Bytes[6];
    Result.Bytes[5] = Result.Bytes[7];
    Result.Bytes[6] = Result.Bytes[7] = Sign;
  }
  if (Count & 8) {
    Result.Bytes[0] = Result.Bytes[1];
    Result.Bytes[1] = Result.Bytes[2];
    Result.Bytes[2] = Result.Bytes[3];
    Result.Bytes[3] = Result.Bytes[4];
    Result.Bytes[4] = Result.Bytes[5];
    Result.Bytes[5] = Result.Bytes[6];
    Result.Bytes[6] = Result.Bytes[7];
    Result.Bytes[7] = Sign;
  }
  for (unsigned I = 0; I != BitShift; ++I) {
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

void *memcpy(void *Destination, const void *Source, uint16_t Count) {
  uint8_t *D = (uint8_t *)Destination;
  const uint8_t *S = (const uint8_t *)Source;
  while (Count--)
    *D++ = *S++;
  return Destination;
}

void *memmove(void *Destination, const void *Source, uint16_t Count) {
  uint8_t *D = (uint8_t *)Destination;
  const uint8_t *S = (const uint8_t *)Source;
  uintptr_t DestinationAddress = (uintptr_t)D;
  uintptr_t SourceAddress = (uintptr_t)S;
  if (!Count || DestinationAddress <= SourceAddress ||
      DestinationAddress - SourceAddress >= Count)
    return memcpy(Destination, Source, Count);

  D += Count;
  S += Count;
  while (Count--)
    *--D = *--S;
  return Destination;
}

void *memset(void *Destination, int Value, uint16_t Count) {
  uint8_t *D = (uint8_t *)Destination;
  uint8_t Byte = (uint8_t)Value;
  while (Count--)
    *D++ = Byte;
  return Destination;
}

int memcmp(const void *LHS, const void *RHS, uint16_t Count) {
  const uint8_t *A = (const uint8_t *)LHS;
  const uint8_t *B = (const uint8_t *)RHS;
  while (Count--) {
    if (*A != *B)
      return (int)*A - (int)*B;
    ++A;
    ++B;
  }
  return 0;
}

uint16_t strlen(const char *String) {
  const char *End = String;
  while (*End)
    ++End;
  return (uint16_t)(End - String);
}

// Generic pointers follow the classic MCS-51 three-byte encoding in the low
// 24 bits: address in bits 0-15 and a memory-space tag in bits 16-23. The
// compiler currently transports this value in a padded i32.
static __attribute__((noinline)) uint8_t
__mcs51_gptr_read_byte(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                       uint8_t Offset) {
  uint16_t Address = (uint16_t)AddressLow | ((uint16_t)AddressHigh << 8);
  Address += Offset;
  if (Tag & 0x80)
    return *(MCS51CodePtr)Address;
  if (!(Tag & 0x40))
    return *(MCS51XDataPtr)Address;
  if (Tag & 0x20)
    return *(MCS51PDataPtr)(uint16_t)(uint8_t)Address;
  return *(MCS51IDataPtr)(uint16_t)(uint8_t)Address;
}

static __attribute__((noinline)) void
__mcs51_gptr_write_byte(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                        uint8_t Offset, uint8_t Value) {
  uint16_t Address = (uint16_t)AddressLow | ((uint16_t)AddressHigh << 8);
  Address += Offset;
  if (Tag & 0x80) {
    // MCS-51 code memory is read-only. Match the established runtime
    // behavior for attempts to write through a code-space generic pointer.
    for (;;) {}
  }
  if (!(Tag & 0x40))
    *(MCS51XDataPtr)Address = Value;
  else if (Tag & 0x20)
    *(MCS51PDataPtr)(uint16_t)(uint8_t)Address = Value;
  else
    *(MCS51IDataPtr)(uint16_t)(uint8_t)Address = Value;
}

uint16_t __mcs51_gptrget8(uint8_t AddressLow, uint8_t AddressHigh,
                          uint8_t Tag, uint8_t Padding) {
  (void)Padding;
  return __mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 0);
}

uint16_t __mcs51_gptrget16(uint8_t AddressLow, uint8_t AddressHigh,
                           uint8_t Tag, uint8_t Padding) {
  (void)Padding;
  return (uint16_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 0) |
         ((uint16_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 1)
          << 8);
}

uint32_t __mcs51_gptrget32(uint8_t AddressLow, uint8_t AddressHigh,
                           uint8_t Tag, uint8_t Padding) {
  (void)Padding;
  return (uint32_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 0) |
         ((uint32_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 1)
          << 8) |
         ((uint32_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 2)
          << 16) |
         ((uint32_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 3)
          << 24);
}

uint64_t __mcs51_gptrget64(uint8_t AddressLow, uint8_t AddressHigh,
                           uint8_t Tag, uint8_t Padding) {
  (void)Padding;
  return (uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 0) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 1)
          << 8) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 2)
          << 16) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 3)
          << 24) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 4)
          << 32) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 5)
          << 40) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 6)
          << 48) |
         ((uint64_t)__mcs51_gptr_read_byte(AddressLow, AddressHigh, Tag, 7)
          << 56);
}

float __mcs51_gptrgetf32(uint8_t AddressLow, uint8_t AddressHigh,
                         uint8_t Tag, uint8_t Padding) {
  (void)Padding;
  Float32Bits Value = {
      .Bits = __mcs51_gptrget32(AddressLow, AddressHigh, Tag, Padding)};
  return Value.Float;
}

void __mcs51_gptrput8(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                     uint8_t Padding, uint8_t Value) {
  (void)Padding;
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 0, Value);
}

void __mcs51_gptrput16(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                       uint8_t Padding, uint8_t ValueLow,
                       uint8_t ValueHigh) {
  (void)Padding;
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 0, ValueLow);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 1, ValueHigh);
}

void __mcs51_gptrput32(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                       uint8_t Padding, uint8_t Value0, uint8_t Value1,
                       uint8_t Value2, uint8_t Value3) {
  (void)Padding;
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 0, Value0);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 1, Value1);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 2, Value2);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 3, Value3);
}

void __mcs51_gptrput64(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                       uint8_t Padding, uint8_t Value0, uint8_t Value1,
                       uint8_t Value2, uint8_t Value3, uint8_t Value4,
                       uint8_t Value5, uint8_t Value6, uint8_t Value7) {
  (void)Padding;
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 0, Value0);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 1, Value1);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 2, Value2);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 3, Value3);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 4, Value4);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 5, Value5);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 6, Value6);
  __mcs51_gptr_write_byte(AddressLow, AddressHigh, Tag, 7, Value7);
}

void __mcs51_gptrputf32(uint8_t AddressLow, uint8_t AddressHigh, uint8_t Tag,
                        uint8_t Padding, uint8_t Value0, uint8_t Value1,
                        uint8_t Value2, uint8_t Value3) {
  (void)Padding;
  __mcs51_gptrput32(AddressLow, AddressHigh, Tag, Padding, Value0, Value1,
                    Value2, Value3);
}
