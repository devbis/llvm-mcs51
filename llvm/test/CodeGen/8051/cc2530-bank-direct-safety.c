// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s

__attribute__((noinline)) unsigned char safe(unsigned char x) { return x + 1; }
// CHECK-LABEL: safe:
// CHECK: .section .text.autobankthunks.direct.{{[0-9]+}},"ax"
// CHECK: __mcs51_bankcall_safe:

__attribute__((noinline)) unsigned char with_asm(unsigned char x) {
  __asm__ volatile ("nop");
  return x;
}
// CHECK-LABEL: with_asm:
// CHECK: .section .text.autobankthunks.{{[0-9]+}},"ax"
// CHECK: __mcs51_bankcall_with_asm:

__attribute__((noinline)) unsigned char variadic(unsigned char x, ...) { return x; }
// CHECK-LABEL: variadic:
// CHECK: .section .text.autobankthunks.{{[0-9]+}},"ax"
// CHECK: __mcs51_bankcall_variadic:
