// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s

// CHECK-LABEL: add_one:
// CHECK: mov a, r7
// CHECK: add a, #1
// CHECK: ret
unsigned char add_one(unsigned char value) { return value + 1; }

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
