// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -S %s -o - | FileCheck %s

__idata volatile unsigned char a;
__idata volatile unsigned char b;
__idata volatile unsigned char result;

void main(void) {
  a = 17;
  b = 9;
  result = (unsigned char)((a * 3u + b) ^ (a - b));
}

// A loaded byte can stay in A through MUL; later IDATA reads feed ALU
// instructions through @R0 directly, without copying each load through a GPR.
// CHECK-LABEL: main:
// CHECK: mov r0, #a
// CHECK-NEXT: mov @r0, #17
// CHECK-NEXT: mov r0, #b
// CHECK-NEXT: mov @r0, #9
// CHECK-NEXT: mov r0, #a
// CHECK-NEXT: mov a, @r0
// CHECK-NEXT: mov 240, #3
// CHECK-NEXT: mul ab
// CHECK-NEXT: mov r0, #b
// CHECK-NEXT: add a, @r0
// CHECK-NEXT: mov r2, a
// CHECK-NEXT: mov r0, #a
// CHECK-NEXT: mov a, @r0
// CHECK-NEXT: mov r0, #b
// CHECK-NEXT: clr c
// CHECK-NEXT: subb a, @r0
// CHECK-NEXT: xrl a, r2
// CHECK-NEXT: mov r0, #result
// CHECK-NEXT: mov @r0, a
// CHECK-NEXT: ret
