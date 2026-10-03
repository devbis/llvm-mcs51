// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o - | FileCheck %s

typedef unsigned char uint8_t;

volatile __xdata uint8_t packet[34];
volatile __xdata uint8_t output[32];
volatile __xdata uint8_t result;

static uint8_t copy_payload(void) {
  uint8_t length = packet[0];
  uint8_t i;
  if (length > 32)
    return 1;
  for (i = 0; i != length; ++i)
    output[i] = packet[2 + i];
  return 0;
}

void main(void) { result = copy_payload(); }

// Pointers live in pairs of direct bytes, so the copy loop moves them into
// DPTR for each access without spilling anything through the stack.
// CHECK-LABEL: main:
// CHECK-NOT: @r1
// CHECK: mov 130, 50
// CHECK: mov 131, 51
// CHECK: movx a, @dptr
// CHECK: mov 130, 48
// CHECK: mov 131, 49
// CHECK: movx @dptr, a
