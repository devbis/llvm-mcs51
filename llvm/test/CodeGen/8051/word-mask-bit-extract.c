// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

unsigned char extract_bit9(unsigned short value) {
  return (value >> 7) & 4;
}

// CHECK-LABEL: extract_bit9:
// CHECK: mov r0, 49
// CHECK-NEXT: mov a, r0
// CHECK-NEXT: anl a, #2
// CHECK-NEXT: clr c
// CHECK-NEXT: rlc a
// CHECK-NEXT: ret
