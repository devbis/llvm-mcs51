// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -ffunction-sections -fdata-sections \
// RUN:   -c %S/../../../lib/Target/MCS51/mcs51-runtime.c -o %t.runtime.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -DTEST_FIX_SFDI -c %s -o %t.user.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,--gc-sections %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.user.o %t.runtime.o -o %t.fixsfdi.elf
// RUN: llvm-readobj --symbols %t.fixsfdi.elf | FileCheck %s --check-prefix=FIX-SFDI
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -DTEST_FIX_UNSFDI -c %s -o %t.user.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,--gc-sections %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.user.o %t.runtime.o -o %t.fixunssfdi.elf
// RUN: llvm-readobj --symbols %t.fixunssfdi.elf | FileCheck %s --check-prefix=FIX-UNSFDI
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -DTEST_FLOATDISF -c %s -o %t.user.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,--gc-sections %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.user.o %t.runtime.o -o %t.floatdisf.elf
// RUN: llvm-readobj --symbols %t.floatdisf.elf | FileCheck %s --check-prefix=FLOATDISF
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -DTEST_FLOATUNDISF -c %s -o %t.user.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,--gc-sections %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.user.o %t.runtime.o -o %t.floatundisf.elf
// RUN: llvm-readobj --symbols %t.floatundisf.elf | FileCheck %s --check-prefix=FLOATUNDISF

__attribute__((noinline)) long long float_to_i64(float value) {
  return (long long)value;
}

__attribute__((noinline)) unsigned long long float_to_u64(float value) {
  return (unsigned long long)value;
}

__attribute__((noinline)) float i64_to_float(long long value) {
  return (float)value;
}

__attribute__((noinline)) float u64_to_float(unsigned long long value) {
  return (float)value;
}

volatile float InputFloat = 1.25f;
volatile long long InputI64 = -16777217;
volatile unsigned long long InputU64 = 16777217;
volatile long long ResultI64;
volatile unsigned long long ResultU64;
volatile float ResultFloat;

int main(void) {
#if defined(TEST_FIX_SFDI)
  ResultI64 = float_to_i64(InputFloat);
#elif defined(TEST_FIX_UNSFDI)
  ResultU64 = float_to_u64(InputFloat);
#elif defined(TEST_FLOATDISF)
  ResultFloat = i64_to_float(InputI64);
#elif defined(TEST_FLOATUNDISF)
  ResultFloat = u64_to_float(InputU64);
#endif
  return 0;
}

// FIX-SFDI-DAG: Name: __fixsfdi
// FIX-UNSFDI-DAG: Name: __fixunssfdi
// FLOATDISF-DAG: Name: __floatdisf
// FLOATUNDISF-DAG: Name: __floatundisf
