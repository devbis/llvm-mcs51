// RUN: clang -target mcs51 -Oz -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o /dev/null

// A general 32-bit multiply calls the runtime; a product of two values that
// fit in 16 bits stays inline (byte products).
unsigned long general(unsigned long a, unsigned long b) { return a * b; }
unsigned long narrow(unsigned short a, unsigned short b) {
  return (unsigned long)a * b;
}

// CHECK-LABEL: general:
// CHECK: lcall __mulsi3
// CHECK-LABEL: narrow:
// CHECK-NOT: lcall __mulsi3
// CHECK: mul ab
