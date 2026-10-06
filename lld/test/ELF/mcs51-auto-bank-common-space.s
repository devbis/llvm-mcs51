# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s

.section .vectors,"ax"
.globl reset
reset:
  nop

# If all available banked flash is full, an auto-banked function should spill
# into the remaining common CODE space rather than overflowing bank 7.
.section .bank1.text,"ax"
.space 0x8000
.section .bank2.text,"ax"
.space 0x8000
.section .bank3.text,"ax"
.space 0x8000
.section .bank4.text,"ax"
.space 0x8000
.section .bank5.text,"ax"
.space 0x8000
.section .bank6.text,"ax"
.space 0x8000
.section .bank7.text,"ax"
.space 0x4800

.section .mcs51.autobank.routine,"ax"
.globl routine
routine:
  nop
  ret

# CHECK: Name: .text
# CHECK: Size: 2
# CHECK: Name: .bank1
# CHECK: Size: 32768
# CHECK: Name: .bank7
# CHECK: Size: 18432
# CHECK: Name: routine
# CHECK: Value: 0x94
