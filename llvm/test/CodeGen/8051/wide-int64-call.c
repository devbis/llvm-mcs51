// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -fno-builtin \
// RUN:   -c %s -o %t.caller.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -fno-builtin \
// RUN:   -DDEFINE_HELPER -c %s -o %t.helper.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s \
// RUN:   %t.caller.o %t.helper.o -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK

#ifdef DEFINE_HELPER
__attribute__((noinline))
unsigned long long helper(unsigned long long value,
                          unsigned short selector) {
  return value + selector;
}
#else
extern unsigned long long helper(unsigned long long value,
                                 unsigned short selector);

unsigned long long forward64(unsigned long long value,
                             unsigned short selector) {
  return helper(value, selector);
}

int main(void) { return (int)forward64(0x123456789abcdef0ull, 0x2345); }
#endif

// CHECK-LABEL: forward64:
// CHECK: lcall __mcs51_bankcall_helper
// CHECK: ret

// LINK-LABEL: <helper>:
// LINK: ret
// LINK-LABEL: <forward64>:
// LINK-COUNT-8: push 224
// LINK: lcall
