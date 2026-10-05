// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffunction-sections -nostdlib \
// RUN:   -Wl,--gc-sections \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s

__attribute__((noinline, section(".mcs51.autobank.relax_target")))
unsigned char relax_target(unsigned char value) {
  return value + 1;
}

__attribute__((noinline, section(".mcs51.autobank.relax_caller")))
unsigned char relax_caller(unsigned char value) {
  return relax_target(value) + 1;
}

int main(void) { return relax_caller(0); }

// The linker assigns both functions to bank 1 and shortens their same-page
// direct call from LCALL to ACALL.
// CHECK-LABEL: <relax_caller>:
// CHECK: acall 32768
