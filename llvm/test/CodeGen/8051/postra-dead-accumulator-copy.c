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

// The byte loads used for the upper result bytes are overwritten while
// extracting the value from DPTR. Keep the stores and skip the dead copies.
// CHECK-LABEL: main:
// CHECK: mov dptr, #result
// CHECK: mov a, r{{[0-7]}}
// CHECK: movx @dptr, a
// CHECK-NEXT: mov a, 131
// CHECK: movx @dptr, a
// CHECK: movx @dptr, a
// CHECK: movx @dptr, a
