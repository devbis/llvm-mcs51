// RUN: clang -target mcs51 -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=O0
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=OPT

unsigned char unsigned_eq(unsigned long lhs, unsigned long rhs) {
  return lhs == rhs;
}

unsigned char unsigned_ne(unsigned long lhs, unsigned long rhs) {
  return lhs != rhs;
}

unsigned char unsigned_lt(unsigned long lhs, unsigned long rhs) {
  return lhs < rhs;
}

unsigned char unsigned_le(unsigned long lhs, unsigned long rhs) {
  return lhs <= rhs;
}

unsigned char unsigned_gt(unsigned long lhs, unsigned long rhs) {
  return lhs > rhs;
}

unsigned char unsigned_ge(unsigned long lhs, unsigned long rhs) {
  return lhs >= rhs;
}

unsigned char signed_lt(long lhs, long rhs) { return lhs < rhs; }
unsigned char signed_le(long lhs, long rhs) { return lhs <= rhs; }
unsigned char signed_gt(long lhs, long rhs) { return lhs > rhs; }
unsigned char signed_ge(long lhs, long rhs) { return lhs >= rhs; }

// Each case exercises the full-width comparison result path at both
// optimization levels, including inversion and operand swapping.
// O0-LABEL: unsigned_eq:
// O0: xrl a,
// O0: orl a,
// O0: ret
// O0-LABEL: unsigned_ne:
// O0: ret
// O0-LABEL: unsigned_lt:
// O0: subb a,
// O0: ret
// O0-LABEL: unsigned_le:
// O0: ret
// O0-LABEL: unsigned_gt:
// O0: ret
// O0-LABEL: unsigned_ge:
// O0: ret
// O0-LABEL: signed_lt:
// O0: subb a,
// O0: xrl a, #128
// O0: ret
// O0-LABEL: signed_le:
// O0: ret
// O0-LABEL: signed_gt:
// O0: ret
// O0-LABEL: signed_ge:
// O0: ret
// OPT-LABEL: unsigned_eq:
// OPT: ret
// OPT-LABEL: unsigned_ne:
// OPT: ret
// OPT-LABEL: unsigned_lt:
// OPT: subb a,
// OPT: ret
// OPT-LABEL: unsigned_le:
// OPT: ret
// OPT-LABEL: unsigned_gt:
// OPT: ret
// OPT-LABEL: unsigned_ge:
// OPT: ret
// OPT-LABEL: signed_lt:
// OPT: xrl a, #128
// OPT: subb a,
// OPT: ret
// OPT-LABEL: signed_le:
// OPT: ret
// OPT-LABEL: signed_gt:
// OPT: ret
// OPT-LABEL: signed_ge:
// OPT: ret
