// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: clang -target mcs51 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=OPT

unsigned short zero_extend_byte(unsigned char value) { return value; }

unsigned short zero_extend_plain_char(char value) { return value; }

short sign_extend_byte(signed char value) { return value; }

unsigned short zero_extend_bool(_Bool value) { return value; }

short sign_extend_before_add(signed char value) { return value + 1; }

unsigned short zero_extend_before_add(unsigned char value) { return value + 1; }

unsigned short zero_extend_plus_constant(unsigned char value) {
  return value + 0x12fe;
}

unsigned short add_unsigned_byte(unsigned short base, unsigned char value) {
  return base + value;
}

short add_signed_byte(short base, signed char value) { return base + value; }

unsigned short subtract_unsigned_byte(unsigned short base,
                                      unsigned char value) {
  return base - value;
}

short subtract_signed_byte(short base, signed char value) {
  return base - value;
}

volatile unsigned char loaded_byte;
volatile signed char loaded_signed_byte;

unsigned short subtract_volatile_byte(unsigned short base) {
  return base - loaded_byte;
}

short subtract_volatile_signed_byte(short base) {
  return base - loaded_signed_byte;
}

// CHECK-LABEL: zero_extend_byte:
// CHECK: mov 48, r7
// CHECK: mov 49, #0
// CHECK: ret
// CHECK-LABEL: zero_extend_plain_char:
// CHECK: mov 48, r7
// CHECK: mov 49, #0
// CHECK: ret
// CHECK-LABEL: sign_extend_byte:
// CHECK: mov 48, r7
// CHECK: mov c, 231
// CHECK: clr a
// CHECK: subb a, #0
// CHECK: mov 49, a
// CHECK: ret
// CHECK-LABEL: zero_extend_bool:
// CHECK: mov 49, #0
// CHECK: ret
// CHECK-LABEL: sign_extend_before_add:
// CHECK: mov c, 231
// CHECK: clr a
// CHECK: subb a, #0
// CHECK: add a, #1
// CHECK: addc a, #0
// CHECK: mov 49, a
// CHECK: ret
// CHECK-LABEL: zero_extend_before_add:
// CHECK: add a, #1
// CHECK: addc a, #0
// CHECK: ret
// CHECK-LABEL: zero_extend_plus_constant:
// CHECK: add a, #254
// CHECK: addc a, #18
// CHECK: mov 49, a
// CHECK: ret
// CHECK-LABEL: add_unsigned_byte:
// CHECK: add a, 48
// CHECK: addc a, #0
// CHECK: ret
// CHECK-LABEL: add_signed_byte:
// CHECK: mov c, 231
// CHECK: add a, 48
// CHECK: addc a, 240
// CHECK: ret
// CHECK-LABEL: subtract_unsigned_byte:
// CHECK: clr c
// CHECK: subb a, r
// CHECK: subb a, #0
// CHECK: ret
// CHECK-LABEL: subtract_signed_byte:
// CHECK: mov c, 231
// CHECK: subb a, r
// CHECK: subb a, 240
// CHECK: ret
// CHECK-LABEL: subtract_volatile_byte:
// CHECK: clr c
// CHECK: subb a, r
// CHECK: subb a, #0
// CHECK: ret
// CHECK-LABEL: subtract_volatile_signed_byte:
// CHECK: mov c, 231
// CHECK: subb a, r
// CHECK: subb a, 240
// CHECK: ret
// OPT-LABEL: zero_extend_byte:
// OPT: mov 48, r7
// OPT: mov 49, #0
// OPT: ret
// OPT-LABEL: zero_extend_plain_char:
// OPT: mov 48, r7
// OPT: mov 49, #0
// OPT: ret
// OPT-LABEL: sign_extend_byte:
// OPT: mov 48, r7
// OPT: mov c, 231
// OPT: clr a
// OPT: subb a, #0
// OPT: mov 49, a
// OPT: ret
// OPT-LABEL: zero_extend_bool:
// OPT: mov 49, #0
// OPT: ret
// OPT-LABEL: sign_extend_before_add:
// OPT: mov c, 231
// OPT: clr a
// OPT: subb a, #0
// OPT: add a, #1
// OPT: addc a, #0
// OPT: mov 49, a
// OPT: ret
// OPT-LABEL: zero_extend_before_add:
// OPT: add a, #1
// OPT: addc a, #0
// OPT: ret
// OPT-LABEL: zero_extend_plus_constant:
// OPT: add a, #254
// OPT: addc a, #18
// OPT: mov 49, a
// OPT: ret
// OPT-LABEL: add_unsigned_byte:
// OPT: add a, 48
// OPT: addc a, #0
// OPT: ret
// OPT-LABEL: add_signed_byte:
// OPT: mov c, 231
// OPT: add a, 48
// OPT: addc a, 240
// OPT: ret
// OPT-LABEL: subtract_unsigned_byte:
// OPT: clr c
// OPT: subb a, r
// OPT: subb a, #0
// OPT: ret
// OPT-LABEL: subtract_signed_byte:
// OPT: mov c, 231
// OPT: subb a, r
// OPT: subb a, 240
// OPT: ret
// OPT-LABEL: subtract_volatile_byte:
// OPT: clr c
// OPT: subb a, r
// OPT: subb a, #0
// OPT: ret
// OPT-LABEL: subtract_volatile_signed_byte:
// OPT: mov c, 231
// OPT: subb a, r
// OPT: subb a, 240
// OPT: ret
