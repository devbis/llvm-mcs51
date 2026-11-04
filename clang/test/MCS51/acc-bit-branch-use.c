// RUN: %clang_cc1 -triple mcs51 -target-cpu cc2530 -Oz -S \
// RUN:   -mllvm -verify-machineinstrs %s -o - | FileCheck %s

// A branch on an ACC bit (JB/JNB 0xE0-0xE7) reads A. Without an implicit use
// of A, the MOV that loads the tested byte is a dead def and
// dead-mi-elimination deletes it, so the branch tests whatever A last held:
// here the shifted and XORed byte, not the original sign bit.

typedef unsigned char u8;
typedef unsigned short u16;

// CHECK-LABEL: crc8:
// CHECK:       mov r[[ORIG:[0-9]]], a
// CHECK-NEXT:  clr c
// CHECK-NEXT:  rlc a
// CHECK-NEXT:  mov r{{[0-9]}}, a
// CHECK-NEXT:  xrl a, #49
// CHECK-NEXT:  mov r{{[0-9]}}, a
// CHECK-NEXT:  mov a, r[[ORIG]]
// CHECK-NEXT:  jnb 231,
u8 crc8(u16 word) {
  u8 c = 0xffu, i;
  for (i = 16; i; i--) {
    if (i == 16u)
      c ^= (u8)(word >> 8);
    else if (i == 8u)
      c ^= (u8)word;
    c = c & 0x80u ? (u8)(c << 1 ^ 0x31u) : (u8)(c << 1);
  }
  return c;
}
