// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -c %s -o %t.first.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DSECOND_TU -c %s -o %t.second.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t.first.o %t.second.o \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s

static __attribute__((noinline)) unsigned char helper(unsigned char value) {
  return value + 1;
}

#ifdef SECOND_TU
unsigned char second_translation_unit(unsigned char value) {
  return helper(value);
}
#else
unsigned char first_translation_unit(unsigned char value) {
  return helper(value);
}
int main(void) { return first_translation_unit(0); }
#endif

// Each object has a private thunk for its same-named static helper.
// CHECK-COUNT-2: Name: __mcs51_bankcall_helper
// CHECK-COUNT-2: Binding: Local
