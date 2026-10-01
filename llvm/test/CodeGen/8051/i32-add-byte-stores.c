// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %t.s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

typedef unsigned char uint8_t;
typedef unsigned long uint32_t;

volatile __xdata uint32_t counter;
volatile __xdata uint8_t bytes[4];

void main(void) {
  uint32_t next = counter + 1ul;
  bytes[0] = (uint8_t)next;
  bytes[1] = (uint8_t)(next >> 8);
  bytes[2] = (uint8_t)(next >> 16);
  bytes[3] = (uint8_t)(next >> 24);
}

// The increment propagates through all four bytes. Truncated byte extracts
// use the byte results directly, so the stores share one incrementing DPTR.
// CHECK-LABEL: main:
// CHECK: mov dptr, #counter
// CHECK: inc r3
// CHECK: mov dptr, #counter+2
// CHECK-NOT: mov @r1, a
// CHECK-NOT: mov a, @r1
// CHECK: mov dptr, #bytes
// CHECK: movx @dptr, a
// CHECK: add a, #1
// CHECK-COUNT-3: addc a, #0
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
