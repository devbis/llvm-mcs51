# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections %t.elf | FileCheck %s

# A bank is filled before the next one is used: the 0x5000 and one 0x2000
# section share bank 1, the other 0x2000 section goes to bank 2.
.section .vectors,"ax"
.globl reset
reset:
  nop

.section .mcs51.autobank.big,"ax"
.space 0x5000
.section .mcs51.autobank.one,"ax"
.space 0x2000
.section .mcs51.autobank.two,"ax"
.space 0x2000

# CHECK: Name: .bank1
# CHECK: Size: 28672
# CHECK: Name: .bank2
# CHECK: Size: 8192
# CHECK: Name: .bank3
# CHECK: Size: 0
