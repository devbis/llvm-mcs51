// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

__attribute__((noinline)) unsigned short
sum4(unsigned short a, unsigned short b, unsigned short c, unsigned short d,
     unsigned short e) {
  return a + b + c + d + e;
}

volatile unsigned short result;

void call_sum4(unsigned short a, unsigned short b,
               unsigned short c, unsigned short d, unsigned short e) {
  result = sum4(a, b, c, d, e);
}

// A 16-bit stack argument is read with a step from its low byte to its high
// byte through R1, without recomputing the address from SP or routing it
// through A.
// CHECK-LABEL: sum4:
// CHECK: mov a, 129
// CHECK: add a, #250
// CHECK: mov r1, a
// CHECK: mov a, @r1
// CHECK: inc r1
// CHECK-NOT: add a, #
// CHECK: mov a, @r1
// CHECK-LABEL: call_sum4:
// CHECK: mov 57, r0
// CHECK: push 57
// CHECK-NEXT: push 56
// CHECK-NEXT: lcall sum4
