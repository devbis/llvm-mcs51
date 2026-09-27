// RUN: clang -target mcs51 -O1 -mllvm -verify-machineinstrs -S %s -o /dev/null
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null

#include <stdint.h>

unsigned long shift_left(unsigned long value, unsigned amount) {
  return value << amount;
}

unsigned long shift_right(unsigned long value, unsigned amount) {
  return value >> amount;
}

long shift_arithmetic(long value, unsigned amount) {
  return value >> amount;
}

uint32_t shift_by_signed_count(uint32_t value, int amount) {
  return amount >= 0 ? value << amount : value >> -amount;
}
