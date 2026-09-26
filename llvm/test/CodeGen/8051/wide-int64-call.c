// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

extern unsigned long long helper(unsigned long long value,
                                 unsigned short selector);

unsigned long long forward64(unsigned long long value,
                             unsigned short selector) {
  return helper(value, selector);
}

// CHECK-LABEL: forward64:
// CHECK: lcall helper
// CHECK: ret
