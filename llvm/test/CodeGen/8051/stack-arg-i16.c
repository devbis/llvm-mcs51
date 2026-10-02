// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

unsigned short sum4(unsigned short a, unsigned short b,
                    unsigned short c, unsigned short d) {
  return a + b + c + d;
}

// Loading a 16-bit stack argument steps from its high byte to its low byte.
// Use the one-byte register decrement instead of routing through A.
// CHECK-LABEL: sum4:
// CHECK: mov r0, a
// CHECK: mov a, @r0
// CHECK: dec r0
// CHECK-NOT: mov a, r0
// CHECK-NOT: dec a
// CHECK: mov a, @r0
