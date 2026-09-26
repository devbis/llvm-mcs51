// RUN: clang -target mcs51 -O0 -mllvm -verify-machineinstrs -S %s -o /dev/null
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null
// RUN: clang -target mcs51 -O0 -S %s -o - | FileCheck %s --check-prefix=ASM

#include <stdarg.h>

unsigned int first_vararg(int count, ...) {
  va_list args;
  va_start(args, count);
  unsigned int value = va_arg(args, unsigned int);
  va_end(args);
  return value;
}

unsigned int call_first_vararg(void) {
  return first_vararg(2, 0x1234u, 0x5678u);
}

unsigned int second_vararg(int count, ...) {
  va_list args;
  va_start(args, count);
  (void)va_arg(args, unsigned int);
  unsigned int value = va_arg(args, unsigned int);
  va_end(args);
  return value;
}

unsigned int call_second_vararg(void) {
  return second_vararg(2, 0x1234u, 0x5678u);
}

unsigned long first_long_vararg(int count, ...) {
  va_list args;
  va_start(args, count);
  unsigned long value = va_arg(args, unsigned long);
  va_end(args);
  return value;
}

unsigned long call_first_long_vararg(void) {
  return first_long_vararg(1, 0x12345678ul);
}

// ASM-LABEL: first_vararg:
// ASM: add a, #-1
// ASM: add a, #-2
// ASM-LABEL: second_vararg:
// ASM: add a, #-1
// ASM: add a, #-2
// ASM: add a, #-1
// ASM: add a, #-2
// ASM-LABEL: first_long_vararg:
// ASM: add a, #-1
// ASM: add a, #-2
// ASM: add a, #-1
// ASM: add a, #-2
