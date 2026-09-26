# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=ELF
# RUN: llvm-objcopy -O ihex %t.elf %t.hex
# RUN: FileCheck %s --check-prefix=HEX < %t.hex

.section .vectors,"ax"
.globl reset
reset:
  nop

.section .bank1.text,"ax"
.globl bank1_entry
bank1_entry:
  .byte 0x11

.section .bank7.text,"ax"
.globl bank7_entry
bank7_entry:
  .byte 0x77

# ELF: Name: .bank1
# ELF: Address: 0x8000
# ELF: Name: .bank7
# ELF: Address: 0x8000
# ELF: Name: bank1_entry
# ELF: Value: 0x8000
# ELF: Name: bank7_entry
# ELF: Value: 0x8000

# Bank 1 and bank 7 share the CPU VMA but retain distinct physical flash LMAs.
# HEX: :01800000116E
# HEX: :020000023000CC
# HEX-NEXT: :018000007708
# HEX: :00000001FF
