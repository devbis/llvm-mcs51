// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s

unsigned char increment_middle(unsigned char lhs, unsigned char rhs) {
  unsigned char value = lhs + 1;
  return value + rhs;
}

unsigned char decrement_middle(unsigned char lhs, unsigned char rhs) {
  unsigned char value = lhs - 1;
  return value + rhs;
}

// CHECK-LABEL: increment_middle:
// CHECK: add a, r
// CHECK-NEXT: inc a
// CHECK-NEXT: ret
// CHECK-NOT: inc r
// CHECK-LABEL: decrement_middle:
// CHECK: dec a
// CHECK-NEXT: ret
