// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s

unsigned long add_long(unsigned long lhs, unsigned long rhs) {
  return lhs + rhs;
}

unsigned long subtract_long(unsigned long lhs, unsigned long rhs) {
  return lhs - rhs;
}

// CHECK-LABEL: add_long:
// CHECK: addc a,
// CHECK: mov r4, a
// CHECK: mov r5, a
// CHECK: mov r6, a
// CHECK: mov r7, a
// CHECK: ret
// CHECK-LABEL: subtract_long:
// CHECK: subb a,
// CHECK: mov r4, a
// CHECK: mov r5, a
// CHECK: mov r6, a
// CHECK: mov r7, a
// CHECK: ret
