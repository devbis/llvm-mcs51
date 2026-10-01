// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffunction-sections -S %s -o - \
// RUN:   | FileCheck %s

__attribute__((noinline)) unsigned char __mcs51_runtime_probe(unsigned char x) {
  return x + 1;
}

// CHECK: .section .text.__mcs51_runtime_probe,"ax"
// CHECK-LABEL: __mcs51_runtime_probe:
