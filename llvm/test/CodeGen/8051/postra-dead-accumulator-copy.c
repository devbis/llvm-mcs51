// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o - | FileCheck %s

typedef unsigned char uint8_t;
typedef unsigned long uint32_t;

volatile __xdata uint32_t counter;
volatile __xdata uint8_t result[4];

void main(void) {
  uint32_t next = counter + 1;
  result[0] = (uint8_t)next;
  result[1] = (uint8_t)(next >> 8);
  result[2] = (uint8_t)(next >> 16);
  result[3] = (uint8_t)(next >> 24);
}

// The byte results are consumed directly, and the four output bytes share a
// single incrementing DPTR.
// CHECK-LABEL: main:
// CHECK: mov dptr, #result
// CHECK: movx @dptr, a
// CHECK: add a, #1
// CHECK-COUNT-3: addc a, #0
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
