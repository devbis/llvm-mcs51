// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o - | FileCheck %s

typedef unsigned long uint32_t;
typedef unsigned char uint8_t;

volatile __xdata uint32_t left_value;
volatile __xdata uint32_t right_value;
volatile __xdata uint8_t result;

void main(void) {
  result = left_value < right_value;
}

// Wide volatile XDATA comparisons use shared target helpers so the caller
// does not keep all operand bytes live across the branch and XDATA accesses.
// CHECK-LABEL: main:
// CHECK: lcall __mcs51_bankcall___mcs51_xdata_ult32
// CHECK: .Lfunc_end
// CHECK-LABEL: __mcs51_xdata_ult32:
// CHECK: movx a, @dptr
