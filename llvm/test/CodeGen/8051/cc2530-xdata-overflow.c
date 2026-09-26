// RUN: clang -target mcs51 -mcpu=cc2530 -c %s -o %t.o
// RUN: not ld.lld -T %S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   --no-check-sections -o %t.elf %t.o 2>&1 | FileCheck %s

// Static XDATA must stop before the 256-byte DATA/IDATA alias at 0x1f00.
unsigned char too_large[0x1f01];

// CHECK: CC2530 .bss overlaps the DATA-space SRAM alias
