// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %t.s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

volatile __data unsigned char counter;
volatile __idata unsigned char idata_counter;

void increment_counter(void) { ++counter; }
void increment_idata_counter(void) { ++idata_counter; }
unsigned char complement_byte(unsigned char value) { return value ^ 0xff; }

// The direct read/modify/write instruction replaces the accumulator round trip.
// CHECK-LABEL: increment_counter:
// CHECK-NOT: mov a, counter
// CHECK: inc counter
// CHECK-NOT: mov a, counter
// CHECK-NOT: mov counter, a
// CHECK-LABEL: increment_idata_counter:
// CHECK-NOT: mov a, @r0
// CHECK: inc @r0
// CHECK-NOT: mov a, @r0
// CHECK-LABEL: complement_byte:
// CHECK: cpl a
// CHECK-NOT: xrl a, #255
