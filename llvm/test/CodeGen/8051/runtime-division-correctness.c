// RUN: cc -O2 -fno-builtin %s -o %t
// RUN: %t

#include <stdint.h>

#include "../../../lib/Target/MCS51/mcs51-runtime.c"

static int check16(uint16_t Numerator, uint16_t Denominator) {
  return Denominator == 0 ||
         (__udivhi3(Numerator, Denominator) == Numerator / Denominator &&
          __umodhi3(Numerator, Denominator) == Numerator % Denominator);
}

static int check32(uint32_t Numerator, uint32_t Denominator) {
  return Denominator == 0 ||
         (__udivsi3(Numerator, Denominator) == Numerator / Denominator &&
          __umodsi3(Numerator, Denominator) == Numerator % Denominator);
}

static int check64(uint64_t Numerator, uint64_t Denominator) {
  return Denominator == 0 ||
         (__udivdi3(Numerator, Denominator) == Numerator / Denominator &&
          __umoddi3(Numerator, Denominator) == Numerator % Denominator);
}

int main(void) {
  static const uint16_t Values16[] = {0, 1, 2, 3, 0x7fff, 0x8000, 0xffff};
  static const uint32_t Values32[] = {
      0, 1, 2, 3, 0x7fffffff, 0x80000000, 0xffffffff, 0xaaaaaaaa};
  static const uint64_t Values64[] = {0,
                                      1,
                                      2,
                                      3,
                                      UINT64_C(0x7fffffffffffffff),
                                      UINT64_C(0x8000000000000000),
                                      UINT64_MAX,
                                      UINT64_C(0xaaaaaaaaaaaaaaaa)};

  for (unsigned I = 0; I != sizeof(Values16) / sizeof(Values16[0]); ++I)
    for (unsigned J = 0; J != sizeof(Values16) / sizeof(Values16[0]); ++J)
      if (!check16(Values16[I], Values16[J]))
        return 1;

  for (unsigned I = 0; I != sizeof(Values32) / sizeof(Values32[0]); ++I)
    for (unsigned J = 0; J != sizeof(Values32) / sizeof(Values32[0]); ++J)
      if (!check32(Values32[I], Values32[J]))
        return 2;

  for (unsigned I = 0; I != sizeof(Values64) / sizeof(Values64[0]); ++I)
    for (unsigned J = 0; J != sizeof(Values64) / sizeof(Values64[0]); ++J)
      if (!check64(Values64[I], Values64[J]))
        return 3;

  return 0;
}
