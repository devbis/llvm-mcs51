// RUN: cc -O2 -fno-builtin %s -o %t
// RUN: %t

#include <stdint.h>
#include <stdio.h>

#include "../../../lib/Target/MCS51/mcs51-runtime.c"

typedef union {
  uint32_t Bits;
  float Float;
} TestFloat32Bits;

static int is_nan(uint32_t Bits) {
  return (Bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) &&
         (Bits & UINT32_C(0x007fffff));
}

static int check(uint32_t LHSBits, uint32_t RHSBits) {
  TestFloat32Bits LHS = {.Bits = LHSBits};
  TestFloat32Bits RHS = {.Bits = RHSBits};
  volatile float ExpectedSum = LHS.Float + RHS.Float;
  volatile float ExpectedDifference = LHS.Float - RHS.Float;
  volatile float ExpectedProduct = LHS.Float * RHS.Float;
  volatile float ExpectedQuotient = LHS.Float / RHS.Float;
  TestFloat32Bits Sum = {.Float = __addsf3(LHS.Float, RHS.Float)};
  TestFloat32Bits Difference = {.Float = __subsf3(LHS.Float, RHS.Float)};
  TestFloat32Bits Product = {.Float = __mulsf3(LHS.Float, RHS.Float)};
  TestFloat32Bits Quotient = {.Float = __divsf3(LHS.Float, RHS.Float)};
  TestFloat32Bits SumExpected = {.Float = ExpectedSum};
  TestFloat32Bits DifferenceExpected = {.Float = ExpectedDifference};
  TestFloat32Bits ProductExpected = {.Float = ExpectedProduct};
  TestFloat32Bits QuotientExpected = {.Float = ExpectedQuotient};

  if (is_nan(SumExpected.Bits) ? !is_nan(Sum.Bits)
                               : SumExpected.Bits != Sum.Bits) {
    fprintf(stderr, "add mismatch: %08x + %08x = %08x, expected %08x\n",
            LHSBits, RHSBits, Sum.Bits, SumExpected.Bits);
    return 0;
  }
  if (is_nan(QuotientExpected.Bits) ? !is_nan(Quotient.Bits)
                                    : QuotientExpected.Bits != Quotient.Bits) {
    fprintf(stderr, "div mismatch: %08x / %08x = %08x, expected %08x\n",
            LHSBits, RHSBits, Quotient.Bits, QuotientExpected.Bits);
    return 0;
  }
  if (is_nan(ProductExpected.Bits) ? !is_nan(Product.Bits)
                                   : ProductExpected.Bits != Product.Bits) {
    fprintf(stderr, "mul mismatch: %08x * %08x = %08x, expected %08x\n",
            LHSBits, RHSBits, Product.Bits, ProductExpected.Bits);
    return 0;
  }
  if (is_nan(DifferenceExpected.Bits)
          ? !is_nan(Difference.Bits)
          : DifferenceExpected.Bits != Difference.Bits) {
    fprintf(stderr, "sub mismatch: %08x - %08x = %08x, expected %08x\n",
            LHSBits, RHSBits, Difference.Bits, DifferenceExpected.Bits);
    return 0;
  }
  return 1;
}

int main(void) {
  static const uint32_t EdgeValues[] = {
      0,          UINT32_C(0x80000000), UINT32_C(0x00000001),
      UINT32_C(0x007fffff), UINT32_C(0x00800000), UINT32_C(0x3f800000),
      UINT32_C(0xbf800000), UINT32_C(0x7f7fffff), UINT32_C(0xff7fffff),
      UINT32_C(0x7f800000), UINT32_C(0xff800000), UINT32_C(0x7fc00001),
      UINT32_C(0xff800123)};
  for (unsigned I = 0; I != sizeof(EdgeValues) / sizeof(EdgeValues[0]); ++I)
    for (unsigned J = 0; J != sizeof(EdgeValues) / sizeof(EdgeValues[0]); ++J)
      if (!check(EdgeValues[I], EdgeValues[J]))
        return 1;

  uint32_t State = UINT32_C(0x12345678);
  for (unsigned I = 0; I != 20000; ++I) {
    State = State * UINT32_C(1664525) + UINT32_C(1013904223);
    uint32_t LHS = State;
    State = State * UINT32_C(1664525) + UINT32_C(1013904223);
    if (!check(LHS, State))
      return 2;
  }
  return 0;
}
