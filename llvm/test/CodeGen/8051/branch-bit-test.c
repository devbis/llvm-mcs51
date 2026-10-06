// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

extern void bit_set(void);
extern void bit_clear(void);

void branch_on_accumulator_bit(unsigned char value) {
  if (value & 0x80)
    bit_set();
  else
    bit_clear();
}

// CHECK-LABEL: branch_on_accumulator_bit:
// CHECK: mov a, r7
// CHECK: jnb 231,
