// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -c %s -o %t.o
// RUN: llvm-readobj --symbols %t.o | FileCheck %s --check-prefix=SYMBOL

__idata volatile unsigned char a;
__idata volatile unsigned char b;
__idata volatile unsigned char result;

void main(void) {
  a = 17;
  b = 9;
  result = (unsigned char)((a * 3u + b) ^ (a - b));
}

// Keep adjacent IDATA globals in one section and walk their addresses with R0.
// CHECK-LABEL: main:
// CHECK: mov r0, #a
// CHECK-NEXT: mov @r0, #17
// CHECK-NEXT: inc r0
// CHECK-NEXT: mov @r0, #9
// CHECK-NEXT: dec r0
// CHECK-NEXT: mov a, @r0
// CHECK-NEXT: mov 240, #3
// CHECK-NEXT: mul ab
// CHECK-NEXT: inc r0
// CHECK-NEXT: add a, @r0
// CHECK-NEXT: mov r2, a
// CHECK-NEXT: dec r0
// CHECK-NEXT: mov a, @r0
// CHECK-NEXT: inc r0
// CHECK-NEXT: clr c
// CHECK-NEXT: subb a, @r0
// CHECK-NEXT: xrl a, r2
// CHECK-NEXT: inc r0
// CHECK-NEXT: mov @r0, a
// CHECK-NEXT: ret

// The R0 increments rely on these same-section offsets.
// SYMBOL: Name: a
// SYMBOL: Value: 0x0
// SYMBOL: Size: 1
// SYMBOL: Section: .mcs51.data2.bss
// SYMBOL: Name: b
// SYMBOL: Value: 0x1
// SYMBOL: Size: 1
// SYMBOL: Section: .mcs51.data2.bss
// SYMBOL: Name: result
// SYMBOL: Value: 0x2
// SYMBOL: Size: 1
// SYMBOL: Section: .mcs51.data2.bss
