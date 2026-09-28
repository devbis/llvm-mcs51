// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: clang -target mcs51 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=OPT

unsigned char unsigned_greater(unsigned char lhs, unsigned char rhs) {
  return lhs > rhs;
}

signed char signed_less(signed char lhs, signed char rhs) {
  return lhs < rhs;
}

unsigned char unsigned_equal(unsigned char lhs, unsigned char rhs) {
  return lhs == rhs;
}

unsigned char unsigned_not_equal(unsigned char lhs, unsigned char rhs) {
  return lhs != rhs;
}

unsigned char equal_zero(unsigned char value) { return value == 0; }

unsigned char not_equal_zero(unsigned char value) { return value != 0; }

unsigned char unsigned_less_equal(unsigned char lhs, unsigned char rhs) {
  return lhs <= rhs;
}

unsigned char unsigned_greater_equal(unsigned char lhs, unsigned char rhs) {
  return lhs >= rhs;
}

unsigned char signed_greater_equal(signed char lhs, signed char rhs) {
  return lhs >= rhs;
}

// CHECK-LABEL: unsigned_greater:
// CHECK: subb a,
// CHECK: rlc a
// CHECK-LABEL: signed_less:
// CHECK: xrl a, #128
// CHECK: xrl a, #128
// CHECK: subb a, 240
// CHECK: rlc a
// CHECK-LABEL: unsigned_equal:
// CHECK: xrl a,
// CHECK: j{{n?z}}
// CHECK-LABEL: unsigned_not_equal:
// CHECK: xrl a,
// CHECK: j{{n?z}}
// CHECK-LABEL: equal_zero:
// CHECK: mov a, r{{[0-7]}}
// CHECK-NOT: xrl a, #0
// CHECK: jnz
// CHECK-LABEL: not_equal_zero:
// CHECK: mov a, r{{[0-7]}}
// CHECK-NOT: xrl a, #0
// CHECK: jnz
// CHECK-LABEL: unsigned_less_equal:
// CHECK: subb a,
// CHECK: cpl c
// CHECK: rlc a
// CHECK-LABEL: unsigned_greater_equal:
// CHECK: subb a,
// CHECK: cpl c
// CHECK: rlc a
// CHECK-LABEL: signed_greater_equal:
// CHECK: xrl a, #128
// CHECK: xrl a, #128
// CHECK: subb a, 240
// CHECK: cpl c
// CHECK: rlc a

// OPT-LABEL: unsigned_greater:
// OPT: subb a,
// OPT: rlc a
// OPT-LABEL: signed_less:
// OPT: xrl a, #128
// OPT: xrl a, #128
// OPT: subb a, 240
// OPT-LABEL: unsigned_equal:
// OPT: xrl a,
// OPT: j{{n?z}}
// OPT-LABEL: unsigned_not_equal:
// OPT: xrl a,
// OPT: j{{n?z}}
// OPT-LABEL: equal_zero:
// OPT: mov a, r{{[0-7]}}
// OPT-NOT: xrl a, #0
// OPT: jnz
// OPT-LABEL: not_equal_zero:
// OPT: mov a, r{{[0-7]}}
// OPT-NOT: xrl a, #0
// OPT: jnz
// OPT-LABEL: unsigned_less_equal:
// OPT: subb a,
// OPT: cpl c
// OPT: rlc a
// OPT-LABEL: unsigned_greater_equal:
// OPT: subb a,
// OPT: cpl c
// OPT: rlc a
// OPT-LABEL: signed_greater_equal:
// OPT: xrl a, #128
// OPT: xrl a, #128
// OPT: subb a, 240
// OPT: cpl c
// OPT: rlc a
