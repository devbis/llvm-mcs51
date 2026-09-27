// RUN: cc -O2 -fno-builtin %s -o %t
// RUN: %t

#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include "../../../lib/Target/MCS51/mcs51-runtime.c"

typedef union {
  uint32_t Bits;
  float Float;
} TestFloat32Bits;

static int is_nan(uint32_t Bits) {
  return (Bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) &&
         (Bits & UINT32_C(0x007fffff));
}

static int32_t expected_float_to_i32(TestFloat32Bits Value) {
  if (is_nan(Value.Bits))
    return Value.Bits >> 31 ? INT32_MIN : INT32_MAX;
  if (Value.Float >= 2147483648.0f)
    return INT32_MAX;
  if (Value.Float <= -2147483648.0f)
    return INT32_MIN;
  return (int32_t)(double)Value.Float;
}

static uint32_t expected_float_to_u32(TestFloat32Bits Value) {
  uint32_t Exponent = (Value.Bits >> 23) & 0xff;
  if ((Value.Bits >> 31) || Exponent < 127)
    return 0;
  if (Exponent >= 159)
    return UINT32_MAX;
  return (uint32_t)(double)Value.Float;
}

static int64_t expected_float_to_i64(TestFloat32Bits Value) {
  if (is_nan(Value.Bits))
    return Value.Bits >> 31 ? INT64_MIN : INT64_MAX;
  if ((double)Value.Float >= 9223372036854775808.0)
    return INT64_MAX;
  if ((double)Value.Float <= -9223372036854775808.0)
    return INT64_MIN;
  return (int64_t)(double)Value.Float;
}

static uint64_t expected_float_to_u64(TestFloat32Bits Value) {
  uint32_t Exponent = (Value.Bits >> 23) & 0xff;
  if ((Value.Bits >> 31) || Exponent < 127)
    return 0;
  if (Exponent >= 191)
    return UINT64_MAX;
  return (uint64_t)(double)Value.Float;
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
  int32_t Converted = __fixsfsi(LHS.Float);
  int32_t ConvertedExpected = expected_float_to_i32(LHS);
  if (Converted != ConvertedExpected) {
    fprintf(stderr, "float-to-int mismatch: %08x = %d, expected %d\n",
            LHSBits, Converted, ConvertedExpected);
    return 0;
  }
  uint32_t UnsignedConverted = __fixunssfsi(LHS.Float);
  uint32_t UnsignedExpected = expected_float_to_u32(LHS);
  if (UnsignedConverted != UnsignedExpected) {
    fprintf(stderr, "float-to-unsigned mismatch: %08x = %u, expected %u\n",
            LHSBits, UnsignedConverted, UnsignedExpected);
    return 0;
  }
  int64_t WideConverted = __fixsfdi(LHS.Float);
  int64_t WideExpected = expected_float_to_i64(LHS);
  if (WideConverted != WideExpected) {
    fprintf(stderr, "float-to-i64 mismatch: %08x = %lld, expected %lld\n",
            LHSBits, (long long)WideConverted, (long long)WideExpected);
    return 0;
  }
  uint64_t WideUnsignedConverted = __fixunssfdi(LHS.Float);
  uint64_t WideUnsignedExpected = expected_float_to_u64(LHS);
  if (WideUnsignedConverted != WideUnsignedExpected) {
    fprintf(stderr, "float-to-u64 mismatch: %08x = %llu, expected %llu\n",
            LHSBits, (unsigned long long)WideUnsignedConverted,
            (unsigned long long)WideUnsignedExpected);
    return 0;
  }

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
  int32_t ExpectedCompare =
      is_nan(LHSBits) || is_nan(RHSBits)
          ? 1
          : LHS.Float < RHS.Float ? -1 : LHS.Float > RHS.Float ? 1 : 0;
  if (__lesf2(LHS.Float, RHS.Float) != ExpectedCompare ||
      __ltsf2(LHS.Float, RHS.Float) != ExpectedCompare ||
      __eqsf2(LHS.Float, RHS.Float) != ExpectedCompare ||
      __nesf2(LHS.Float, RHS.Float) != ExpectedCompare) {
    fprintf(stderr, "ordered compare mismatch: %08x, %08x\n", LHSBits,
            RHSBits);
    return 0;
  }
  int32_t ExpectedGreater =
      is_nan(LHSBits) || is_nan(RHSBits) ? -1 : ExpectedCompare;
  if (__gesf2(LHS.Float, RHS.Float) != ExpectedGreater ||
      __gtsf2(LHS.Float, RHS.Float) != ExpectedGreater) {
    fprintf(stderr, "greater compare mismatch: %08x, %08x\n", LHSBits,
            RHSBits);
    return 0;
  }
  if (__unordsf2(LHS.Float, RHS.Float) !=
      (is_nan(LHSBits) || is_nan(RHSBits))) {
    fprintf(stderr, "unordered compare mismatch: %08x, %08x\n", LHSBits,
            RHSBits);
    return 0;
  }
  return 1;
}

static int check_integer_to_float(int32_t SignedValue, uint32_t UnsignedValue,
                                  int64_t WideSignedValue,
                                  uint64_t WideUnsignedValue) {
  TestFloat32Bits SignedActual = {.Float = __floatsisf(SignedValue)};
  TestFloat32Bits SignedExpected = {.Float = (float)SignedValue};
  if (SignedActual.Bits != SignedExpected.Bits) {
    fprintf(stderr, "signed int-to-float mismatch: %d = %08x, expected %08x\n",
            SignedValue, SignedActual.Bits, SignedExpected.Bits);
    return 0;
  }

  TestFloat32Bits UnsignedActual = {.Float = __floatunsisf(UnsignedValue)};
  TestFloat32Bits UnsignedExpected = {.Float = (float)UnsignedValue};
  if (UnsignedActual.Bits != UnsignedExpected.Bits) {
    fprintf(stderr, "unsigned int-to-float mismatch: %u = %08x, expected %08x\n",
            UnsignedValue, UnsignedActual.Bits, UnsignedExpected.Bits);
    return 0;
  }

  TestFloat32Bits WideSignedActual = {.Float = __floatdisf(WideSignedValue)};
  TestFloat32Bits WideSignedExpected = {.Float = (float)WideSignedValue};
  if (WideSignedActual.Bits != WideSignedExpected.Bits) {
    fprintf(stderr, "signed i64-to-float mismatch: %lld = %08x, expected %08x\n",
            (long long)WideSignedValue, WideSignedActual.Bits,
            WideSignedExpected.Bits);
    return 0;
  }

  TestFloat32Bits WideUnsignedActual = {
      .Float = __floatundisf(WideUnsignedValue)};
  TestFloat32Bits WideUnsignedExpected = {.Float = (float)WideUnsignedValue};
  if (WideUnsignedActual.Bits != WideUnsignedExpected.Bits) {
    fprintf(stderr, "unsigned i64-to-float mismatch: %llu = %08x, expected %08x\n",
            (unsigned long long)WideUnsignedValue, WideUnsignedActual.Bits,
            WideUnsignedExpected.Bits);
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

  static const uint64_t WideEdges[] = {
      0, 1, UINT64_C(0x00ffffff), UINT64_C(0x01000000),
      UINT64_C(0x01000001), UINT64_C(0x7fffffffffffffff),
      UINT64_C(0x8000000000000000), UINT64_MAX};
  for (unsigned I = 0; I != sizeof(WideEdges) / sizeof(WideEdges[0]); ++I)
    for (unsigned J = 0; J != sizeof(WideEdges) / sizeof(WideEdges[0]); ++J)
      if (!check_integer_to_float((int32_t)WideEdges[I], (uint32_t)WideEdges[J],
                                  (int64_t)WideEdges[I], WideEdges[J]))
        return 1;

  static const int32_t SignedEdges[] = {
      INT32_MIN, INT32_MIN + 1, -16777217, -16777216, -1, 0, 1,
      16777215, 16777216, 16777217, INT32_MAX};
  static const uint32_t UnsignedEdges[] = {
      0, 1, 16777215, 16777216, 16777217, UINT32_C(0x7fffffff),
      UINT32_C(0x80000000), UINT32_MAX};
  for (unsigned I = 0; I != sizeof(SignedEdges) / sizeof(SignedEdges[0]); ++I)
    for (unsigned J = 0; J != sizeof(UnsignedEdges) / sizeof(UnsignedEdges[0]);
         ++J)
      if (!check_integer_to_float(SignedEdges[I], UnsignedEdges[J],
                                  SignedEdges[I], UnsignedEdges[J]))
        return 1;

  uint32_t State = UINT32_C(0x12345678);
  for (unsigned I = 0; I != 20000; ++I) {
    State = State * UINT32_C(1664525) + UINT32_C(1013904223);
    int32_t SignedValue = (int32_t)State;
    uint32_t UnsignedValue = State;
    State = State * UINT32_C(1664525) + UINT32_C(1013904223);
    uint64_t WideValue = ((uint64_t)State << 32) | UnsignedValue;
    if (!check_integer_to_float(SignedValue, UnsignedValue,
                                (int64_t)WideValue, WideValue))
      return 3;
    uint32_t LHS = State;
    State = State * UINT32_C(1664525) + UINT32_C(1013904223);
    if (!check(LHS, State))
      return 2;
  }
  return 0;
}
