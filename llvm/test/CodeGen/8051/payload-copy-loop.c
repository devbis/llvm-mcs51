// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %t.s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

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

// The XDATA cursors live in pairs of direct bytes for the whole loop: they
// are loaded from symbols once and only stepped inside it, with no frame slots.
// CHECK-LABEL: main:
// CHECK: mov 48, #lo8(output)
// CHECK: mov 49, #hi8(output)
// CHECK: mov 50, #lo8(packet+2)
// CHECK: mov 51, #hi8(packet+2)
// CHECK-NOT: @r1
// CHECK: mov 130, 50
// CHECK: mov 131, 51
// CHECK: movx a, @dptr
// CHECK: mov 130, 48
// CHECK: mov 131, 49
// CHECK: movx @dptr, a
// CHECK: inc 50
// CHECK: inc 48
