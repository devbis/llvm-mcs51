/* Minimal arithmetic runtime for targets without a system libgcc. */
#include <stdint.h>

typedef union {
  uint64_t Value;
  uint8_t Bytes[8];
} Word64Bytes;

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
    unsigned Carry = Numerator >> 63;
    Numerator <<= 1;
    Rest = (Rest << 1) | Carry;
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
    if (Carry || GreaterOrEqual) {
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
    unsigned Carry = Numerator >> 15;
    Numerator <<= 1;
    Rest = (uint16_t)((Rest << 1) | Carry);
    if (Carry || Rest >= Denominator) {
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
    unsigned Carry = Numerator >> 31;
    Numerator <<= 1;
    Rest = (Rest << 1) | Carry;
    uint16_t RestHigh = (uint16_t)(Rest >> 16);
    uint16_t RestLow = (uint16_t)Rest;
    if (Carry || RestHigh > DenominatorHigh ||
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
