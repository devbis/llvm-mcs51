# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s

# The seven bank groups exceed bank 7's IAR-compatible 0x4800-byte region by
# one byte. Since the common window has room, the second placement pass moves
# that group there instead of writing into the reserved Z-Stack flash range.

.section .vectors,"ax"
.globl reset
reset:
  nop

.section .text.main,"ax"
  ret

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
.section .mcs51.autobank.fallback,"ax"
.globl fallback_to_common
fallback_to_common:
  .space 0x4801

# CHECK: Name: .bank7
# CHECK: Size: 0
# CHECK: Name: fallback_to_common
# CHECK: Value: 0x95
