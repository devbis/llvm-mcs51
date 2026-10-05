// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -c %s -o %t.first.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DSECOND_TU -c %s -o %t.second.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t.first.o %t.second.o \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

static __attribute__((noinline, section(".mcs51.autobank.helper")))
unsigned char helper(unsigned char value) {
  return value + 1;
}
typedef unsigned char (*helper_callback_t)(unsigned char);
static helper_callback_t volatile helper_callback = helper;

#ifdef SECOND_TU
unsigned char second_translation_unit(unsigned char value) {
  return helper_callback(value);
}
#else
unsigned char first_translation_unit(unsigned char value) {
  return helper_callback(value);
}
unsigned char second_translation_unit(unsigned char value);
int main(void) {
  return first_translation_unit(0) + second_translation_unit(0);
}
#endif

// Each translation unit keeps a private thunk for its same-named static
// helper. The linked image contains both bodies even though the symbol table
// prints only one of the duplicate local names.
// CHECK: Name: __mcs51_bankcall_helper
// CHECK: Binding: Local
// DIS-COUNT-2: <__mcs51_bankcall_helper>:
