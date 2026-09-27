// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s
// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s --check-prefix=O0
// RUN: clang -target mcs51 -S -O1 -mllvm -verify-machineinstrs %s -o /dev/null
// RUN: clang -target mcs51 -S -O1 %s -o %t.s
// RUN: llvm-mc -triple=mcs51 -filetype=obj %t.s -o %t.o

unsigned long add_long(unsigned long lhs, unsigned long rhs) {
  return lhs + rhs;
}

unsigned long subtract_long(unsigned long lhs, unsigned long rhs) {
  return lhs - rhs;
}

unsigned short multiply_word(unsigned short lhs, unsigned short rhs) {
  return lhs * rhs;
}

unsigned short multiply_bytes(unsigned char lhs, unsigned char rhs) {
  return lhs * rhs;
}

unsigned long multiply_long(unsigned long lhs, unsigned long rhs) {
  return lhs * rhs;
}

unsigned long shift_right_long(unsigned long value, unsigned char amount) {
  return value >> amount;
}

unsigned long shift_left_long(unsigned long value, unsigned char amount) {
  return value << amount;
}

long shift_right_signed_long(long value, unsigned char amount) {
  return value >> amount;
}

unsigned int shift_right_word(unsigned int value, unsigned int amount) {
  return value >> amount;
}

unsigned int shift_left_word(unsigned int value, unsigned int amount) {
  return value << amount;
}

int shift_right_signed_word(int value, unsigned int amount) {
  return value >> amount;
}

unsigned char less_than_unsigned_long(unsigned long lhs, unsigned long rhs) {
  return lhs < rhs;
}

unsigned char less_than_signed_long(long lhs, long rhs) {
  return lhs < rhs;
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
// CHECK-LABEL: multiply_bytes:
// CHECK-COUNT-1: mul ab
// CHECK: ret
// CHECK-LABEL: multiply_long:
// CHECK: mul ab
// CHECK: ret
// CHECK-LABEL: shift_right_long:
// CHECK: jz
// CHECK-COUNT-4: rrc a
// CHECK: djnz 240,
// CHECK: ret
// CHECK-LABEL: shift_left_long:
// CHECK-COUNT-4: rlc a
// CHECK: djnz 240,
// CHECK: ret
// CHECK-LABEL: shift_right_signed_long:
// CHECK: mov c, 231
// CHECK-COUNT-4: rrc a
// CHECK: djnz 240,
// CHECK: ret
// CHECK-LABEL: shift_right_word:
// CHECK: ret
// CHECK: rrc a
// CHECK: djnz r0,
// CHECK: sjmp
// CHECK-LABEL: shift_left_word:
// CHECK: ret
// CHECK: rlc a
// CHECK: djnz r0,
// CHECK: sjmp
// CHECK-LABEL: shift_right_signed_word:
// CHECK: ret
// CHECK: mov c, 231
// CHECK: rrc a
// CHECK: djnz r0,
// CHECK: sjmp
// CHECK-LABEL: less_than_unsigned_long:
// CHECK: subb a,
// CHECK: rlc a
// CHECK: ret
// CHECK-LABEL: less_than_signed_long:
// CHECK: subb a,
// CHECK: xrl a, #128
// CHECK: rlc a
// CHECK: ret

// O0-LABEL: add_long:
// O0-COUNT-3: addc a,
// O0-NOT: subb a,
// O0: ret
// O0-LABEL: subtract_long:
// O0-COUNT-4: subb a,
// O0: ret
