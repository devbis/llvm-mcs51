// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -S %s -o - | FileCheck %s

unsigned long long add64(unsigned long long lhs, unsigned long long rhs) {
  return lhs + rhs;
}

unsigned long long multiply64(unsigned long long lhs,
                              unsigned long long rhs) {
  return lhs * rhs;
}

unsigned char less_than_unsigned64(unsigned long long lhs,
                                  unsigned long long rhs) {
  return lhs < rhs;
}

unsigned char less_than_signed64(long long lhs, long long rhs) {
  return lhs < rhs;
}

unsigned char equal64(unsigned long long lhs, unsigned long long rhs) {
  return lhs == rhs;
}

// CHECK-LABEL: add64:
// CHECK: addc a,
// CHECK-LABEL: multiply64:
// CHECK: mul ab
// CHECK-LABEL: less_than_unsigned64:
// CHECK-NOT: lcall
// CHECK: ret
// CHECK-LABEL: less_than_signed64:
// CHECK-NOT: lcall
// CHECK: ret
// CHECK-LABEL: equal64:
// CHECK-NOT: lcall
// CHECK: ret
