// RUN: not clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf 2>&1 \
// RUN:   | FileCheck %s

void __attribute__((interrupt(1))) first(void) {}
void __attribute__((interrupt(1))) second(void) {}
int main(void) { return 0; }

// CHECK: CC2530 interrupt vector 1 has multiple handlers
