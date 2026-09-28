// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -S %s -o - | FileCheck %s

__data volatile unsigned char a;
__data volatile unsigned char b;
__data volatile unsigned char result;

void main(void) {
  a = 17;
  b = 9;
  result = (unsigned char)((a * 3u + b) ^ (a - b));
}

// Keep volatile reads in source order while operating directly on DATA.
// CHECK-LABEL: main:
// CHECK: mov a, #17
// CHECK-NEXT: mov b, #9
// CHECK-NEXT: mov a, a
// CHECK-NEXT: mov 240, #3
// CHECK-NEXT: mul ab
// CHECK-NEXT: add a, b
// CHECK-NEXT: mov r0, a
// CHECK-NEXT: mov a, a
// CHECK-NEXT: clr c
// CHECK-NEXT: subb a, b
// CHECK-NEXT: xrl a, r0
// CHECK-NEXT: mov result, a
// CHECK-NEXT: ret
