/* Minimal arithmetic runtime for targets without a system libgcc. */
#include <stdint.h>

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
