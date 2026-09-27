// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin -mllvm -verify-machineinstrs -c %s -o %t.user.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -ffunction-sections -fdata-sections -mllvm -verify-machineinstrs \
// RUN:   -c %S/../../../lib/Target/MCS51/mcs51-runtime.c -o %t.runtime.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,--gc-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.user.o %t.runtime.o -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK

__attribute__((noinline)) signed char
divide_signed_byte(signed char Numerator, signed char Denominator) {
  return Numerator / Denominator;
}

__attribute__((noinline)) signed char
remainder_signed_byte(signed char Numerator, signed char Denominator) {
  return Numerator % Denominator;
}

int main(void) {
  volatile signed char Numerator = -73;
  volatile signed char Denominator = 9;
  return divide_signed_byte(Numerator, Denominator) +
         remainder_signed_byte(Numerator, Denominator);
}

// CHECK-LABEL: divide_signed_byte:
// Signed char operands are integer-promoted before division in C.
// CHECK: lcall __divhi3
// CHECK-LABEL: remainder_signed_byte:
// CHECK: lcall __modhi3

// LINK-DAG: Name: __divhi3
// LINK-DAG: Name: __modhi3
