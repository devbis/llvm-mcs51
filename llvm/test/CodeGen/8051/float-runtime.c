// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -mllvm -verify-machineinstrs -c %s -o %t.user.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -ffunction-sections -fdata-sections -mllvm -verify-machineinstrs \
// RUN:   -c %S/../../../lib/Target/MCS51/mcs51-runtime.c \
// RUN:   -o %t.runtime.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,--gc-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.user.o %t.runtime.o -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK

__attribute__((noinline)) float add_float(float lhs, float rhs) {
  return lhs + rhs;
}

__attribute__((noinline)) float subtract_float(float lhs, float rhs) {
  return lhs - rhs;
}

__attribute__((noinline)) float multiply_float(float lhs, float rhs) {
  return lhs * rhs;
}

__attribute__((noinline)) float divide_float(float lhs, float rhs) {
  return lhs / rhs;
}

__attribute__((noinline)) int float_less(float lhs, float rhs) {
  return lhs < rhs;
}

__attribute__((noinline)) long float_to_long(float value) {
  return (long)value;
}

__attribute__((noinline)) unsigned long float_to_unsigned_long(float value) {
  return (unsigned long)value;
}

__attribute__((noinline)) float long_to_float(long value) {
  return (float)value;
}

__attribute__((noinline)) float unsigned_long_to_float(unsigned long value) {
  return (float)value;
}

volatile float Result;
volatile long IntegerResult;
volatile unsigned long UnsignedIntegerResult;

int main(void) {
  Result = add_float(1.25f, 2.5f);
  Result = subtract_float(Result, 1.0f);
  Result = multiply_float(Result, 0.5f);
  Result = divide_float(Result, 2.0f);
  IntegerResult = float_to_long(Result);
  UnsignedIntegerResult = float_to_unsigned_long(Result);
  Result = long_to_float(IntegerResult);
  Result = unsigned_long_to_float(UnsignedIntegerResult);
  if (float_less(Result, 0.0f))
    Result = 0.0f;
  return 0;
}

// LINK-DAG: Name: __addsf3
// LINK-DAG: Name: __subsf3
// LINK-DAG: Name: __mulsf3
// LINK-DAG: Name: __divsf3
// LINK-DAG: Name: __ltsf2
// LINK-DAG: Name: __unordsf2
// LINK-DAG: Name: __fixsfsi
// LINK-DAG: Name: __fixunssfsi
// LINK-DAG: Name: __floatsisf
// LINK-DAG: Name: __floatunsisf
