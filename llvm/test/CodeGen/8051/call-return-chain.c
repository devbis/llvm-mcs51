// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

extern unsigned char leaf(unsigned char);

unsigned char forward_return(unsigned char value) {
  return leaf(value);
}

// CHECK-LABEL: forward_return:
// CHECK: lcall leaf
// CHECK: ret
