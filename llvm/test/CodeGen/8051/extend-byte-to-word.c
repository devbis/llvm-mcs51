// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: clang -target mcs51 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=OPT

unsigned short zero_extend_byte(unsigned char value) { return value; }

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
// CHECK-LABEL: sign_extend_before_add:
// CHECK: mov c, 231
// CHECK: clr a
// CHECK: subb a, #0
// CHECK: mov 131, a
// CHECK: inc dptr
// CHECK: ret
// CHECK-LABEL: zero_extend_before_add:
// CHECK: mov 131, #0
// CHECK: inc dptr
// CHECK: ret
// CHECK-LABEL: zero_extend_plus_constant:
// CHECK: add a, #254
// CHECK: addc a, #18
// CHECK: mov 131, a
// CHECK: ret
// OPT-LABEL: sign_extend_before_add:
// OPT: mov c, 231
// OPT: clr a
// OPT: subb a, #0
// OPT: mov 131, a
// OPT: inc dptr
// OPT: ret
// CHECK-LABEL: add_unsigned_byte:
// CHECK: add a, 130
// CHECK: addc a, #0
// CHECK: ret
// CHECK-LABEL: add_signed_byte:
// CHECK: add a, 130
// CHECK: addc a, #0
// CHECK: jnc
// CHECK: add a, #255
// CHECK: ret
// CHECK-LABEL: subtract_unsigned_byte:
// CHECK: clr c
// CHECK: subb a, r
// CHECK: subb a, #0
// CHECK: ret
// CHECK-LABEL: subtract_signed_byte:
// CHECK: clr c
// CHECK: subb a, r
// CHECK: subb a, #0
// CHECK: jnc
// CHECK: add a, #1
// CHECK: ret
// OPT-LABEL: zero_extend_before_add:
// OPT: mov 131, #0
// OPT: inc dptr
// OPT: ret
// OPT-LABEL: zero_extend_plus_constant:
// OPT: add a, #254
// OPT: addc a, #18
// OPT: mov 131, a
// OPT: ret
// OPT-LABEL: add_unsigned_byte:
// OPT: add a, 130
// OPT: addc a, #0
// OPT: ret
// OPT-LABEL: add_signed_byte:
// OPT: add a, 130
// OPT: addc a, #0
// OPT: jnc
// OPT: add a, #255
// OPT: ret
// OPT-LABEL: subtract_unsigned_byte:
// OPT: clr c
// OPT: subb a, r7
// OPT: subb a, #0
// OPT: ret
// OPT-LABEL: subtract_signed_byte:
// OPT: clr c
// OPT: subb a, r7
// OPT: subb a, #0
// OPT: jnc
// OPT: add a, #1
// OPT: ret
// OPT-LABEL: subtract_volatile_byte:
// OPT: clr c
// OPT: subb a, r
// OPT: subb a, #0
// OPT: ret
// OPT-LABEL: subtract_volatile_signed_byte:
// OPT: clr c
// OPT: subb a, r
// OPT: subb a, #0
// OPT: jnc
// OPT: add a, #1
// OPT: ret
