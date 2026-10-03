// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
// RUN:   -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -fno-builtin \
// RUN:   -c %s -o %t.caller.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -fno-builtin \
// RUN:   -DDEFINE_HELPER -c %s -o %t.helper.o
// RUN: clang -target mcs51 -mcpu=cc2530 %t.caller.o %t.helper.o -o %t.elf
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

__attribute__((noinline))
unsigned long long forward64_again(unsigned long long value,
                                   unsigned short selector) {
  return helper(value, selector);
}

int main(void) { return (int)forward64(0x123456789abcdef0ull, 0x2345); }
#endif

// CHECK-LABEL: forward64:
// CHECK: lcall __mcs51_bankcall_helper
// CHECK: ret
// CHECK: .weak __mcs51_bankcall_helper
// CHECK-LABEL: forward64_again:
// CHECK: lcall __mcs51_bankcall_helper
// CHECK: ret
// CHECK-NOT: .weak __mcs51_bankcall_helper

// LINK-LABEL: <helper>:
// LINK: lcall
// LINK: ljmp
// LINK-LABEL: <forward64>:
// LINK: mov 57, r2
// LINK: push 57
// LINK-NEXT: push 56
// LINK-NEXT: push 55
// LINK-NEXT: push 54
// LINK-NEXT: push 53
// LINK-NEXT: push 52
// LINK-NEXT: push 51
// LINK-NEXT: push 50
// LINK-NEXT: lcall
