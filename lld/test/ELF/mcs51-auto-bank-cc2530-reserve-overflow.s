# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: not ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o 2>&1 | FileCheck %s

# Fill the common window so that an autobank group that exceeds the reserved
# bank-7 region cannot be moved there. The final link must reject the image.

.section .vectors,"ax"
.globl reset
reset:
  nop

.section .text.main,"ax"
  .space 0x7f6c

.section .mcs51.autobank.full1,"ax"
  .space 0x8000
.section .mcs51.autobank.full2,"ax"
  .space 0x8000
.section .mcs51.autobank.full3,"ax"
  .space 0x8000
.section .mcs51.autobank.full4,"ax"
  .space 0x8000
.section .mcs51.autobank.full5,"ax"
  .space 0x8000
.section .mcs51.autobank.full6,"ax"
  .space 0x8000
.section .mcs51.autobank.overflow,"ax"
  .space 0x4801

# CHECK: CC2530 bank 7 overlaps reserved Z-Stack flash
