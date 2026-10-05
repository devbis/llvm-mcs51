// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -mllvm -verify-machineinstrs -S %s -o /dev/null

__attribute__((noinline)) unsigned long outline_first(unsigned long value) {
  value = (value << 8) | (value >> 24);
  value += 0x13579bdfu;
  value ^= 0x2468ace0u;
  value += value >> 7;
  return value + 1;
}

__attribute__((noinline)) unsigned long outline_second(unsigned long value) {
  value = (value << 8) | (value >> 24);
  value += 0x13579bdfu;
  value ^= 0x2468ace0u;
  value += value >> 7;
  return value + 2;
}

volatile __xdata unsigned char shared_state;

__attribute__((noinline)) unsigned char outline_xdata_first(void) {
  unsigned char value = shared_state;
  value = (value << 1) | (value >> 7);
  shared_state = value;
  value ^= 0x5a;
  return value + 1;
}

__attribute__((noinline)) unsigned char outline_xdata_second(void) {
  unsigned char value = shared_state;
  value = (value << 1) | (value >> 7);
  shared_state = value;
  value ^= 0x5a;
  return value + 2;
}

// CHECK-LABEL: outline_first:
// CHECK: lcall OUTLINED_FUNCTION_0
// CHECK-LABEL: outline_second:
// CHECK: lcall OUTLINED_FUNCTION_0
// CHECK-LABEL: outline_xdata_first:
// CHECK: lcall OUTLINED_FUNCTION_
// CHECK-LABEL: outline_xdata_second:
// CHECK: lcall OUTLINED_FUNCTION_
// CHECK: .section .text.__mcs51_outlined.OUTLINED_FUNCTION_{{[0-9]+}},"ax"
// CHECK: OUTLINED_FUNCTION_0:
// CHECK: ret
// CHECK-NOT: lcall __mcs51_bankcall_OUTLINED_FUNCTION
