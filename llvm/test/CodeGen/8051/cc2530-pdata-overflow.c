// PDATA pointers hold only an 8-bit offset within the XDATA page selected by
// MPAGE, so every PDATA object must fit in that page.  A single object larger
// than the page is rejected by Clang (see clang/test/MCS51/pdata-object-size.c);
// objects that are individually valid can still overflow the page together,
// which the linker script must diagnose.
// RUN: clang -target mcs51 -mcpu=cc2530 -DPART=1 -c %s -o %t1.o
// RUN: clang -target mcs51 -mcpu=cc2530 -DPART=2 -c %s -o %t2.o
// RUN: clang -target mcs51 -mcpu=cc2530 -DPART=3 -c %s -o %t3.o
// RUN: not ld.lld -T %S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   --no-check-sections -o %t.elf %t1.o %t2.o 2>&1 | FileCheck %s
// RUN: ld.lld -T %S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   --no-check-sections -o %t.fit.elf %t1.o %t3.o
// RUN: llvm-readobj --symbols %t.fit.elf | FileCheck %s --check-prefix=FIT

#if PART == 1
__pdata unsigned char first_half[200];
int main(void) { return 0; }
#elif PART == 2
__pdata unsigned char second_half[57];
#else
__pdata unsigned char remainder[56];
#endif

// CHECK: CC2530 PDATA globals exceed the MPAGE zero page

// FIT: Name: __mcs51_pdata_bss_end
// FIT-NEXT: Value: 0x100
