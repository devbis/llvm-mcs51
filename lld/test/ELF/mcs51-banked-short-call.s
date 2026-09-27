# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: not ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o 2>&1 | FileCheck %s

.section .text.main,"ax"
.globl entry
.type entry,@function
entry:
  acall banked_target
  ret

.section .mcs51.autobank.target,"ax"
.globl banked_target
.type banked_target,@function
banked_target:
  ret

# CHECK: MCS-51 AJMP/ACALL cannot cross from bank 0 to bank 1 for function 'banked_target'
