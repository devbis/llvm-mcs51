// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -c %s -o %t.caller.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -DDEFINE_CALLEE -c %s -o %t.callee.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %t.caller.o %t.callee.o \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld -Wl,--no-check-sections -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s

#ifdef DEFINE_CALLEE
__attribute__((noinline)) unsigned char callee(unsigned char x) { return x + 1; }
#else
unsigned char callee(unsigned char);
typedef unsigned char (*fn)(unsigned char);
fn volatile callback = callee;
__attribute__((noinline, section(".bank1.caller")))
unsigned char caller(unsigned char x) { return callee(x); }
int main(void) { return caller(1); }
#endif

// CHECK-LABEL: <__mcs51_bankcall_callee>:
// CHECK: push 159
// CHECK: lcall {{3[2-9][0-9][0-9][0-9]}}
// CHECK-LABEL: <caller>:
// CHECK: lcall {{3[2-9][0-9][0-9][0-9]}}
// CHECK-LABEL: <callee>:
