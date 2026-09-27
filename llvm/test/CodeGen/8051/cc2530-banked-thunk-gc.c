// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -nostdlib -Wl,--gc-sections \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s

__attribute__((noinline, section(".bank1.text")))
unsigned char live_banked_function(unsigned char value) {
  return value + 1;
}

__attribute__((noinline, section(".bank2.text")))
unsigned char dead_banked_function(unsigned char value) {
  return value + 2;
}

__attribute__((noinline))
unsigned char caller(unsigned char value) {
  return live_banked_function(value);
}

int main(void) { return caller(0); }

// CHECK: <__mcs51_bankcall_live_banked_function>:
// CHECK-NOT: <__mcs51_bankcall_dead_banked_function>:
// CHECK: <live_banked_function>:
// CHECK-NOT: <dead_banked_function>:
