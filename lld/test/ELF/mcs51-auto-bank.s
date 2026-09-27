# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s

.section .vectors,"ax"
.globl reset
reset:
  nop

.section .mcs51.autobank.alpha,"ax"
.globl auto_alpha
auto_alpha:
  nop
  ret

.section .mcs51.autobank.beta,"ax"
.globl auto_beta
auto_beta:
  nop
  ret

.section .mcs51.autobank.gamma,"ax"
.globl auto_gamma
auto_gamma:
  nop
  ret

# CHECK: Name: .bank1
# CHECK: Size: 2
# CHECK: Name: .bank2
# CHECK: Size: 2
# CHECK: Name: .bank3
# CHECK: Size: 2
# CHECK: Name: auto_alpha
# CHECK: Value: 0x8000
# CHECK: Name: auto_beta
# CHECK: Value: 0x8000
# CHECK: Name: auto_gamma
# CHECK: Value: 0x8000
# CHECK-NOT: Name: .mcs51.autobank.
