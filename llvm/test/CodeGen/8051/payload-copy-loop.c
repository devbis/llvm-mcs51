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

// Frame slots holding the XDATA cursors are adjacent. Track direct R1 steps
// through the loop instead of rebuilding each address from SP.
// CHECK-LABEL: main:
// CHECK: mov dptr, #packet+2
// CHECK: mov r1, a
// CHECK: mov @r1, a
// CHECK: inc r1
// CHECK: mov @r1, a
// CHECK: movx a, @dptr
// CHECK: inc r1
// CHECK: mov a, @r1
// CHECK: mov 130, a
// CHECK: inc r1
// CHECK: mov a, @r1
// CHECK: mov 131, a
// CHECK: movx @dptr, a
// CHECK: dec r1
// CHECK-COUNT-2: dec r1
