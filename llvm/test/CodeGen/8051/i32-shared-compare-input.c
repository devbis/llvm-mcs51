// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o - | FileCheck %s

typedef unsigned long uint32_t;

volatile __xdata uint32_t incoming;
volatile __xdata uint32_t stored;

unsigned char incoming_is_max_or_older(void) {
  uint32_t value = incoming;
  return value == 0xfffffffful || value < stored;
}

// Reusing the captured bytes of `value` across both comparisons avoids
// spilling the shared volatile load to the internal stack.
// CHECK-LABEL: incoming_is_max_or_older:
// CHECK-NOT: inc 129
// CHECK: cjne a, #255
// CHECK-NOT: inc 129
// CHECK: .Lfunc_end
