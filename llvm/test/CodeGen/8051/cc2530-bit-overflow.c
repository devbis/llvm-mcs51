// RUN: clang -target mcs51 -mcpu=cc2530 -ffreestanding -c %s -o %t.o
// RUN: not clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld -Wl,--no-check-sections %S/../../../lib/Target/MCS51/cc2530_startup.s %t.o -o %t.elf 2>&1 | FileCheck %s

__bit volatile unsigned char too_many_bits[129];

int main(void) { return 0; }

// CHECK: CC2530 bit globals exceed the bit-addressable DATA area
