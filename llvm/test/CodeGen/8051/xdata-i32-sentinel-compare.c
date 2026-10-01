// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o - | FileCheck %s

typedef unsigned long uint32_t;
typedef unsigned char uint8_t;

volatile __xdata uint32_t incoming_counter;
volatile __xdata uint32_t stored_counter;
volatile __xdata uint8_t result;

void main(void) {
  uint32_t incoming = incoming_counter;
  result = incoming == 0xfffffffful || incoming < stored_counter;
}

// Keep the sentinel short circuit in a helper: stored_counter must not be
// read when incoming_counter is UINT32_MAX.
// CHECK-LABEL: main:
// CHECK: lcall __mcs51_xdata_ult32_or_max
// CHECK: .Lfunc_end
// CHECK-LABEL: __mcs51_xdata_ult32_or_max:
// CHECK: movx a, @dptr
// CHECK: cjne a, #255
