// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -c %s -o %t.caller.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DDEFINE_BANKED -c %s -o %t.callee.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t.caller.o \
// RUN:   %t.callee.o -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S %s -o - \
// RUN:   | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DOMIT_BANK_ATTRIBUTE -c %s \
// RUN:   -o %t.bad-caller.o
// RUN: not clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t.bad-caller.o \
// RUN:   %t.callee.o -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -o %t.bad.elf 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ERR

#ifdef DEFINE_BANKED
__attribute__((noinline, section(".bank2.text")))
unsigned char banked_add(unsigned char value) { return value + 1; }
#else
#ifndef OMIT_BANK_ATTRIBUTE
__attribute__((section(".bank2.text")))
#endif
unsigned char banked_add(unsigned char value);

typedef unsigned char (*callback_t)(unsigned char);
callback_t volatile banked_callback = banked_add;

unsigned char cross_tu_caller(unsigned char value) { return banked_add(value); }
int main(void) { return cross_tu_caller(0); }
#endif

// LINK-LABEL: <cross_tu_caller>:
// LINK: lcall
// LINK-LABEL: <__mcs51_bankcall_banked_add>:
// LINK: mov 159, #2
// LINK: lcall 32768

// ASM-LABEL: cross_tu_caller:
// ASM: lcall __mcs51_bankcall_banked_add
// ASM: .short __mcs51_bankcall_banked_add

// A banked definition without a matching section attribute at the call site
// would otherwise produce a silent cross-bank call to the shared code VMA.
// ERR: MCS-51 reference to banked function 'banked_add' must use its bank-call trampoline
