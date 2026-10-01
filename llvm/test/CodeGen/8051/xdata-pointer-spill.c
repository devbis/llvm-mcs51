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

// XDATA pointer spills use direct-to-indirect moves instead of routing each
// byte through A while saving and restoring DPTR.
// CHECK-LABEL: main:
// CHECK: mov @r1, 130
// CHECK: mov @r1, 131
// CHECK: mov 130, @r1
// CHECK: mov 131, @r1
