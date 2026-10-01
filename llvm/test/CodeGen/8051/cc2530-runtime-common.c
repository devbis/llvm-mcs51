// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffunction-sections -S %s -o - \
// RUN:   | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffunction-sections -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s \
// RUN:   %S/cc2530-runtime-common-runtime.c -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK

extern unsigned char __mcs51_runtime_probe(unsigned char x);

__attribute__((noinline)) unsigned char user_probe(unsigned char x) {
  return x + 2;
}

__attribute__((section(".bank3.text")))
unsigned char call_probes(unsigned char x) {
  return __mcs51_runtime_probe(x) + user_probe(x);
}

int main(void) { return call_probes(1); }

// The runtime symbol is placed in a regular function section but stays in the
// common window. Its call is direct; ordinary user code remains auto-banked.
// ASM-LABEL: call_probes:
// ASM: lcall __mcs51_runtime_probe
// ASM: lcall __mcs51_bankcall_user_probe
// ASM-NOT: __mcs51_bankcall___mcs51_runtime_probe
// LINK: Disassembly of section .text:
// LINK: <__mcs51_bankcall_user_probe>:
// LINK: <__mcs51_runtime_probe>:
// LINK: Disassembly of section .bank1:
// LINK: <user_probe>:
