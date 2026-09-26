// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: clang -target mcs51 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=OPT

unsigned short zero_extend_byte(unsigned char value) { return value; }

short sign_extend_byte(signed char value) { return value; }

unsigned short zero_extend_bool(_Bool value) { return value; }

// CHECK-LABEL: zero_extend_byte:
// CHECK: mov 130, a
// CHECK: mov 131, #0
// CHECK-LABEL: sign_extend_byte:
// CHECK: mov 130, a
// CHECK: mov c, 231
// CHECK: clr a
// CHECK: subb a, #0
// CHECK: mov 131, a
// CHECK-LABEL: zero_extend_bool:
// CHECK: mov 131, #0

// OPT-LABEL: zero_extend_byte:
// OPT: mov 130, a
// OPT: mov 131, #0
// OPT-LABEL: sign_extend_byte:
// OPT: mov 130, a
// OPT: mov c, 231
// OPT: clr a
// OPT: subb a, #0
// OPT: mov 131, a
// OPT-LABEL: zero_extend_bool:
// OPT: mov 131, #0
