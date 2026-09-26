// RUN: clang -target mcs51 -mcpu=cc2530 -c %s -o %t.o
// RUN: not ld.lld -T %S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   --no-check-sections -o %t.elf %t.o 2>&1 | FileCheck %s

// PDATA pointers have only an 8-bit offset within the selected MPAGE page.
__pdata unsigned char too_large[257];

// CHECK: CC2530 PDATA globals exceed the MPAGE zero page
