// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O1 -mllvm -verify-machineinstrs -S %s -o /dev/null

// CHECK-LABEL: add_one:
// CHECK: mov a, r7
// CHECK: inc a
// CHECK-NEXT: ret
unsigned char add_one(unsigned char value) { return value + 1; }

// CHECK-LABEL: subtract_one:
// CHECK: mov a, r7
// CHECK: dec a
// CHECK: ret
unsigned char subtract_one(unsigned char value) { return value - 1; }

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

// CHECK-LABEL: multiply_values:
// CHECK: mov a, r6
// CHECK: mov 240, r7
// CHECK: mul ab
// CHECK: ret
unsigned char multiply_values(unsigned char lhs, unsigned char rhs) {
  return lhs * rhs;
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

// CHECK-LABEL: shift_left_two:
// CHECK: mov a, r7
// CHECK: clr c
// CHECK: rlc a
// CHECK: clr c
// CHECK: rlc a
// CHECK: ret
unsigned char shift_left_two(unsigned char value) {
  return (unsigned char)(value << 2);
}

// CHECK-LABEL: shift_right_two:
// CHECK: mov a, r7
// CHECK: clr c
// CHECK: rrc a
// CHECK: clr c
// CHECK: rrc a
// CHECK: ret
unsigned char shift_right_two(unsigned char value) {
  return (unsigned char)(value >> 2);
}

// CHECK-LABEL: arithmetic_shift_one:
// CHECK: mov a, r7
// CHECK-NEXT: mov c, 231
// CHECK-NEXT: rrc a
// CHECK-NEXT: ret
signed char arithmetic_shift_one(signed char value) { return value >> 1; }

// CHECK-LABEL: arithmetic_shift_two:
// CHECK: mov a, r7
// CHECK-NEXT: mov c, 231
// CHECK-NEXT: rrc a
// CHECK-NEXT: mov c, 231
// CHECK-NEXT: rrc a
// CHECK-NEXT: ret
signed char arithmetic_shift_two(signed char value) { return value >> 2; }

// CHECK-LABEL: arithmetic_shift_variable:
// CHECK: mov c, 231
// CHECK: djnz
signed char arithmetic_shift_variable(signed char value,
                                       unsigned char amount) {
  return value >> amount;
}

// CHECK-LABEL: logical_shift_variable:
// CHECK: clr c
// CHECK: rrc a
// CHECK: djnz
unsigned char logical_shift_variable(unsigned char value,
                                     unsigned char amount) {
  return value >> amount;
}

// CHECK-LABEL: left_shift_variable:
// CHECK: clr c
// CHECK: rlc a
// CHECK: djnz
unsigned char left_shift_variable(unsigned char value, unsigned char amount) {
  return value << amount;
}

// CHECK-LABEL: add_words:
// CHECK: add a,
// CHECK: addc a,
// CHECK: ret
unsigned short add_words(unsigned short lhs, unsigned short rhs) {
  return lhs + rhs;
}

// CHECK-LABEL: subtract_words:
// CHECK: clr c
// CHECK: subb a,
// CHECK: subb a,
// CHECK: ret
unsigned short subtract_words(unsigned short lhs, unsigned short rhs) {
  return lhs - rhs;
}

// CHECK-LABEL: less_than_words:
// CHECK: subb a,
// CHECK: subb a,
// CHECK: rlc a
// CHECK: ret
unsigned char less_than_words(unsigned short lhs, unsigned short rhs) {
  return lhs < rhs;
}

// CHECK-LABEL: less_equal_words:
// CHECK: subb a,
// CHECK: subb a,
// CHECK: xrl a, #1
// CHECK: ret
unsigned char less_equal_words(unsigned short lhs, unsigned short rhs) {
  return lhs <= rhs;
}

// CHECK-LABEL: greater_words:
// CHECK: subb a,
// CHECK: subb a,
// CHECK: ret
unsigned char greater_words(unsigned short lhs, unsigned short rhs) {
  return lhs > rhs;
}

// CHECK-LABEL: greater_equal_words:
// CHECK: subb a,
// CHECK: subb a,
// CHECK: xrl a, #1
// CHECK: ret
unsigned char greater_equal_words(unsigned short lhs, unsigned short rhs) {
  return lhs >= rhs;
}

// CHECK-LABEL: signed_less_words:
// CHECK: xrl a,
// CHECK: xch a, 240
// CHECK: xrl a,
// CHECK: subb a, 240
// CHECK: ret
unsigned char signed_less_words(short lhs, short rhs) { return lhs < rhs; }

// CHECK-LABEL: signed_less_equal_words:
// CHECK: xch a, 240
// CHECK: xrl a,
// CHECK: ret
unsigned char signed_less_equal_words(short lhs, short rhs) {
  return lhs <= rhs;
}

// CHECK-LABEL: signed_greater_words:
// CHECK: xch a, 240
// CHECK: subb a, 240
// CHECK: ret
unsigned char signed_greater_words(short lhs, short rhs) {
  return lhs > rhs;
}

// CHECK-LABEL: signed_greater_equal_words:
// CHECK: xrl a,
// CHECK: xch a, 240
// CHECK: xrl a,
// CHECK: ret
unsigned char signed_greater_equal_words(short lhs, short rhs) {
  return lhs >= rhs;
}
