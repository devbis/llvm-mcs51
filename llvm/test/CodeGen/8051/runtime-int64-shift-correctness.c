// RUN: cc -O2 -fno-builtin %s -o %t
// RUN: %t

#include <stdint.h>

#include "../../../lib/Target/MCS51/mcs51-runtime.c"

static int check(uint64_t Value, int Count) {
  uint64_t ExpectedLeft = Count >= 64 ? 0 : Value << Count;
  uint64_t ExpectedLogicalRight = Count >= 64 ? 0 : Value >> Count;
  uint64_t ExpectedArithmeticRight;
  if (Count >= 64)
    ExpectedArithmeticRight = (Value >> 63) ? UINT64_MAX : 0;
  else if (Count == 0)
    ExpectedArithmeticRight = Value;
  else {
    ExpectedArithmeticRight = Value >> Count;
    if (Value >> 63)
      ExpectedArithmeticRight |= UINT64_MAX << (64 - Count);
  }

  if (__ashldi3(Value, Count) != ExpectedLeft ||
      __lshrdi3(Value, Count) != ExpectedLogicalRight ||
      (uint64_t)__ashrdi3((int64_t)Value, Count) != ExpectedArithmeticRight)
    return 0;
  return 1;
}

int main(void) {
  static const uint64_t Edges[] = {
      0, 1, UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
      UINT64_MAX, UINT64_C(0x0123456789abcdef),
      UINT64_C(0xfedcba9876543210)};
  static const int Counts[] = {0, 1, 7, 8, 15, 16, 31, 32, 47, 63, 64, 65, 127};
  for (unsigned I = 0; I != sizeof(Edges) / sizeof(Edges[0]); ++I)
    for (unsigned J = 0; J != sizeof(Counts) / sizeof(Counts[0]); ++J)
      if (!check(Edges[I], Counts[J]))
        return 1;

  uint64_t State = UINT64_C(0x123456789abcdef0);
  for (unsigned I = 0; I != 20000; ++I) {
    State = State * UINT64_C(6364136223846793005) + 1;
    if (!check(State, I % 80))
      return 2;
  }
  return 0;
}
