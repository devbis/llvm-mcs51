// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %t.s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

typedef unsigned char uint8_t;

volatile __xdata uint8_t selector;
volatile __xdata uint8_t result;

static uint8_t choose_result(uint8_t value) {
  if (value < 4)
    return 1;
  if (value < 10)
    return 2;
  return 3;
}

void main(void) { result = choose_result(selector); }

// The selected byte is carried in R0. Dead accumulator materializations of
// the same constants must not survive on the three return edges.
// CHECK-LABEL: main:
// CHECK: mov dptr, #selector
// CHECK: subb a, #10
// CHECK: mov r{{[0-7]}}, #2
// CHECK-NOT: mov a, #2
// CHECK: mov r{{[0-7]}}, #3
// CHECK-NOT: mov a, #3
// CHECK: subb a, #4
// CHECK: mov r{{[0-7]}}, #1
// CHECK-NOT: mov a, #1
// CHECK: mov dptr, #result
// CHECK: mov a, r{{[0-7]}}
// CHECK: movx @dptr, a
