// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: clang -target mcs51 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=OPT

unsigned char unsigned_greater_equal16(unsigned short lhs,
                                       unsigned short rhs) {
  return lhs >= rhs;
}

unsigned char unsigned_less_equal16(unsigned short lhs,
                                    unsigned short rhs) {
  return lhs <= rhs;
}

unsigned char signed_greater_equal16(short lhs, short rhs) {
  return lhs >= rhs;
}

unsigned char signed_less_equal16(short lhs, short rhs) {
  return lhs <= rhs;
}

// CHECK-LABEL: unsigned_greater_equal16:
// CHECK: subb a,
// CHECK: cpl c
// CHECK: rlc a
// CHECK-NOT: xrl a, #1
// CHECK-LABEL: unsigned_less_equal16:
// CHECK: subb a,
// CHECK: cpl c
// CHECK: rlc a
// CHECK-NOT: xrl a, #1
// CHECK-LABEL: signed_greater_equal16:
// CHECK: subb a, 240
// CHECK: cpl c
// CHECK: rlc a
// CHECK-NOT: xrl a, #1
// CHECK-LABEL: signed_less_equal16:
// CHECK: subb a, 240
// CHECK: cpl c
// CHECK: rlc a
// CHECK-NOT: xrl a, #1

// OPT-LABEL: unsigned_greater_equal16:
// OPT: subb a,
// OPT: cpl c
// OPT: rlc a
// OPT-NOT: xrl a, #1
// OPT-LABEL: unsigned_less_equal16:
// OPT: subb a,
// OPT: cpl c
// OPT: rlc a
// OPT-NOT: xrl a, #1
// OPT-LABEL: signed_greater_equal16:
// OPT: subb a, 240
// OPT: cpl c
// OPT: rlc a
// OPT-NOT: xrl a, #1
// OPT-LABEL: signed_less_equal16:
// OPT: subb a, 240
// OPT: cpl c
// OPT: rlc a
// OPT-NOT: xrl a, #1
