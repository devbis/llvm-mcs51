// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %t.s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

typedef unsigned char uint8_t;

volatile __idata uint8_t selector;
volatile __idata uint8_t result;

void main(void) {
  selector = 7;
  if (selector < 4)
    result = 1;
  else if (selector < 10)
    result = 2;
  else
    result = 3;
}

// Keep the selected store on each return edge instead of joining the values
// into a PHI and storing after the branches.
// CHECK-LABEL: main:
// CHECK: mov @r0, #7
// CHECK: subb a, #4
// CHECK: mov @r0, #1
// CHECK: ret
// CHECK: subb a, #10
// CHECK: mov @r0, #2
// CHECK: ret
// CHECK: mov @r0, #3
// CHECK: ret
