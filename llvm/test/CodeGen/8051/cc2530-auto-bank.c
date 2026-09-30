// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-readobj --sections %t.elf | FileCheck %s --check-prefix=MAP
// RUN: clang -target mcs51 -mcpu=cc2530 -flto -c %s -o %t-lto.o
// RUN: clang -target mcs51 -mcpu=cc2530 -flto -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t-lto.o -o %t-lto.elf

typedef unsigned char (*callback_t)(unsigned char);

__attribute__((section(".mcs51.autobank.banked_add")))
unsigned char banked_add(unsigned char value);

__attribute__((noinline, section(".mcs51.autobank.banked_add")))
unsigned char banked_add(unsigned char value) { return value + 1; }

__attribute__((section(".mcs51.autobank.banked_xor")))
unsigned char banked_xor(unsigned char value);

__attribute__((noinline, section(".mcs51.autobank.banked_xor")))
unsigned char banked_xor(unsigned char value) { return value ^ 0x5a; }

__attribute__((noinline, section(".text.lto_internal.llvm.123")))
unsigned char lto_internal(unsigned char value) { return value + 3; }

callback_t volatile banked_callback = banked_add;

__attribute__((noinline))
unsigned char call_banked(unsigned char value) { return banked_add(value); }

__attribute__((noinline))
unsigned char call_other_bank(unsigned char value) {
  return banked_xor(value);
}

int main(void) {
  return call_banked(1) + call_other_bank(2) + lto_internal(3);
}

// ASM: .section .mcs51.autobank.banked_add,"ax"
// ASM-LABEL: banked_add:
// ASM: .section .text.autobankthunks.{{[0-9]+}},"ax"
// ASM-LABEL: __mcs51_bankcall_banked_add:
// ASM: push 159
// ASM: mov 159, #banked_add
// ASM: lcall banked_add
// ASM: pop 159
// ASM: ret
// ASM: .section .text.autobankthunks.{{[0-9]+}},"ax"
// ASM-LABEL: __mcs51_bankcall_banked_xor:
// ASM: mov 159, #banked_xor
// ASM: lcall banked_xor
// ASM: .section .text.lto_internal.llvm.123,"ax"
// ASM-LABEL: lto_internal:
// ASM: .section .text.autobankthunks.
// ASM-LABEL: __mcs51_bankcall_lto_internal:
// ASM-LABEL: call_banked:
// ASM: lcall __mcs51_bankcall_banked_add
// ASM: .short __mcs51_bankcall_banked_add

// MAP: Name: .bank1
// MAP: Address: 0x8000
// MAP: Size: {{[1-9][0-9]*}}
// MAP: Name: .bank2
// MAP: Address: 0x8000
// MAP: Size: {{[1-9][0-9]*}}
// LINK-LABEL: <__mcs51_bankcall_banked_add>:
// LINK: push 159
// LINK: mov 159, #{{[1-7]}}
// LINK: lcall 32768
// LINK: pop 159
// LINK: ret
// LINK-LABEL: <__mcs51_bankcall_banked_xor>:
// LINK: mov 159, #{{[1-7]}}
// LINK-LABEL: <__mcs51_bankcall_call_banked>:
// LINK: mov 159, #{{[1-7]}}
// LINK: lcall 32768
// LINK-LABEL: <__mcs51_bankcall_call_other_bank>:
// LINK: mov 159, #{{[1-7]}}
