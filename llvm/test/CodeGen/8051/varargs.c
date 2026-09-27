// RUN: clang -target mcs51 -O0 -mllvm -verify-machineinstrs -S %s -o /dev/null
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null
// RUN: clang -target mcs51 -O0 -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s --check-prefix=ASM-O2

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

unsigned long long first_long_long_vararg(int count, ...) {
  va_list args;
  va_start(args, count);
  unsigned long long value = va_arg(args, unsigned long long);
  va_end(args);
  return value;
}

unsigned long long call_first_long_long_vararg(void) {
  return first_long_long_vararg(1, 0x123456789abcdef0ull);
}

double first_double_vararg(int count, ...) {
  va_list args;
  va_start(args, count);
  double value = va_arg(args, double);
  va_end(args);
  return value;
}

double call_first_double_vararg(void) {
  return first_double_vararg(1, 1.5);
}

__attribute__((noinline)) float echo_float(float value) { return value; }

float call_echo_float(void) { return echo_float(2.5f); }

// ASM-LABEL: first_vararg:
// ASM: add a, #-1
// ASM: add a, #-2
// ASM-O2-LABEL: first_double_vararg:
// ASM-O2: mov r4, a
// ASM-O2: mov r5, a
// ASM-O2: mov r6, a
// ASM-O2: mov r7, a
// ASM-O2: ret
// ASM-O2-LABEL: call_first_double_vararg:
// ASM-O2: mov a, #63
// ASM-O2: push 224
// ASM-O2: mov a, #-64
// ASM-O2: push 224
// ASM-O2: mov a, #0
// ASM-O2: push 224
// ASM-O2: push 224
// ASM-O2: mov a, 131
// ASM-O2: push 224
// ASM-O2: mov a, 130
// ASM-O2: push 224
// ASM-O2: lcall first_double_vararg
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
