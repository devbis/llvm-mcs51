# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s

.section .vectors,"ax"
.globl reset
reset:
  nop

# Leave only three bytes in bank 1. The two branch-connected sections total
# four bytes, so they must be assigned to bank 2 together.
.section .mcs51.autobank.filler,"ax"
.space 32765

.section .mcs51.autobank.alpha,"ax"
.globl auto_alpha
auto_alpha:
  sjmp auto_beta
  ret

.section .mcs51.autobank.beta,"ax"
.globl auto_beta
auto_beta:
  ret

# CHECK: Name: .bank1
# CHECK: Size: 32765
# CHECK: Name: .bank2
# CHECK: Size: 4
# CHECK: Name: auto_alpha
# CHECK: Value: 0x8000
# CHECK: Name: auto_beta
# CHECK: Value: 0x8003
# CHECK-NOT: Name: .mcs51.autobank.
