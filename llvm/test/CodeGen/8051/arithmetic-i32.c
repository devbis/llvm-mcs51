// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s
// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s --check-prefix=O0

unsigned long add_long(unsigned long lhs, unsigned long rhs) {
  return lhs + rhs;
}

unsigned long subtract_long(unsigned long lhs, unsigned long rhs) {
  return lhs - rhs;
}

unsigned short multiply_word(unsigned short lhs, unsigned short rhs) {
  return lhs * rhs;
}

unsigned long multiply_long(unsigned long lhs, unsigned long rhs) {
  return lhs * rhs;
}

// CHECK-LABEL: add_long:
// CHECK-COUNT-3: addc a,
// CHECK-NOT: subb a,
// CHECK: mov r4, a
// CHECK: mov r5, a
// CHECK: mov r6, a
// CHECK: mov r7, a
// CHECK: ret
// CHECK-LABEL: subtract_long:
// CHECK-COUNT-4: subb a,
// CHECK: mov r4, a
// CHECK: mov r5, a
// CHECK: mov r6, a
// CHECK: mov r7, a
// CHECK: ret
// CHECK-LABEL: multiply_word:
// CHECK: mul ab
// CHECK: mul ab
// CHECK: mul ab
// CHECK: ret
// CHECK-LABEL: multiply_long:
// CHECK: mul ab
// CHECK: ret

// O0-LABEL: add_long:
// O0-COUNT-3: addc a,
// O0-NOT: subb a,
// O0: ret
// O0-LABEL: subtract_long:
// O0-COUNT-4: subb a,
// O0: ret
