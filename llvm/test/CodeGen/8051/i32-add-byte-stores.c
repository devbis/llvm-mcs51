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

// The increment is propagated through all four bytes, and the byte stores do
// not spill their 32-bit result words to the stack.
// CHECK-LABEL: main:
// CHECK: mov dptr, #counter+2
// CHECK: mov dptr, #counter
// CHECK: add a, #1
// CHECK: addc a, #0
// CHECK: addc a, #0
// CHECK: addc a, #0
// CHECK-NOT: mov @r1, a
// CHECK-NOT: mov a, @r1
// CHECK: mov dptr, #bytes
// CHECK: movx @dptr, a
// The four contiguous byte stores reuse DPTR instead of reconstructing each
// shifted byte through DPL/DPH and reloading the destination address.
// CHECK-NOT: mov 130, a
// CHECK-NOT: mov 131, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
