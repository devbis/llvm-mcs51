// RUN: clang -target mcs51 -mcpu=cc2530 -c %s -o %t.o
// RUN: not ld.lld -T %S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   --no-check-sections -o %t.elf %t.o 2>&1 | FileCheck %s

typedef unsigned char idata8 __attribute__((address_space(2)));
idata8 too_large[0x51];

// CHECK: CC2530 DATA/IDATA globals overlap the hardware stack
