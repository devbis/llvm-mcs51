// RUN: %clang -target mcs51 -S -O1 %s -o - | FileCheck %s

// CHECK-LABEL: add_one:
// CHECK: mov a, r7
// CHECK: add a, #1
// CHECK: ret
unsigned char add_one(unsigned char value) { return value + 1; }

// CHECK-LABEL: add_values:
// CHECK: mov a, r6
// CHECK: add a, r7
// CHECK: ret
unsigned char add_values(unsigned char lhs, unsigned char rhs) {
  return lhs + rhs;
}

// CHECK-LABEL: subtract_values:
// CHECK: mov a, r7
// CHECK: clr c
// CHECK: subb a, r6
// CHECK: ret
unsigned char subtract_values(unsigned char lhs, unsigned char rhs) {
  return lhs - rhs;
}

// CHECK-LABEL: mask_low:
// CHECK: mov a, r7
// CHECK: anl a, #15
// CHECK: ret
unsigned char mask_low(unsigned char value) { return value & 15; }

// CHECK-LABEL: set_low:
// CHECK: mov a, r7
// CHECK: orl a, #15
// CHECK: ret
unsigned char set_low(unsigned char value) { return value | 15; }

// CHECK-LABEL: toggle_low:
// CHECK: mov a, r7
// CHECK: xrl a, #15
// CHECK: ret
unsigned char toggle_low(unsigned char value) { return value ^ 15; }
