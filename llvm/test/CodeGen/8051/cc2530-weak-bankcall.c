// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -c %s -o %t.caller.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DWEAK_TARGET -c %s -o %t.target1.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DWEAK_TARGET -c %s -o %t.target2.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t.caller.o \
// RUN:   %t.target1.o %t.target2.o \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s

#ifdef WEAK_TARGET
__attribute__((weak, noinline))
unsigned char bank_target(unsigned char value) { return value + 1; }
#else
extern unsigned char bank_target(unsigned char value);
__attribute__((noinline))
unsigned char call_bank_target(unsigned char value) {
  return bank_target(value);
}
int main(void) { return call_bank_target(0); }
#endif

// CHECK: Name: __mcs51_bankcall_bank_target
// CHECK: Binding: Weak
