# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=REL
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-readobj --sections %t.elf | FileCheck %s --check-prefix=MAP
# RUN: llvm-objdump -d %t.elf | FileCheck %s

# A trampoline for a local (static) function names its callee through the
# callee's STT_SECTION symbol. The bank byte must still be the callee's bank.
.section .vectors,"ax"
.globl reset
reset:
  lcall __mcs51_bankcall_helper
  lcall __mcs51_bankcall_global_helper
  ret

.section .mcs51.autobank.fill,"ax"
.space 0x7ff0

.section .mcs51.autobank.helper,"ax"
.type helper,@function
helper:
  .space 0x20
  ret

.section .mcs51.autobank.global_helper,"ax"
.globl global_helper
.type global_helper,@function
global_helper:
  ret

.section .text.autobankthunks.0,"ax"
.local __mcs51_bankcall_helper
__mcs51_bankcall_helper:
  push 159
  mov 159, #helper
  lcall helper
  pop 159
  ret

.section .text.autobankthunks.1,"ax"
.globl __mcs51_bankcall_global_helper
__mcs51_bankcall_global_helper:
  push 159
  mov 159, #global_helper
  lcall global_helper
  pop 159
  ret

# REL:      Section ({{.*}}) .rela.text.autobankthunks.0 {
# REL-NEXT:   0x4 R_8051_8 .mcs51.autobank.helper 0x0
# REL-NEXT:   0x6 R_8051_16_BE .mcs51.autobank.helper 0x0
# REL:      Section ({{.*}}) .rela.text.autobankthunks.1 {
# REL-NEXT:   0x4 R_8051_8 global_helper 0x0
# REL-NEXT:   0x6 R_8051_16_BE global_helper 0x0

# First fit: global_helper still fits in bank 1, helper goes to bank 2.
# MAP:      Name: .bank1
# MAP:      Size: 32753
# MAP:      Name: .bank2
# MAP:      Size: 33

# CHECK:      <__mcs51_bankcall_helper>:
# CHECK-NEXT:   c0 9f    push 159
# CHECK-NEXT:   75 9f 02 mov 159, #2
# CHECK-NEXT:   12 80 00 lcall 32768
# CHECK:      <__mcs51_bankcall_global_helper>:
# CHECK-NEXT:   c0 9f    push 159
# CHECK-NEXT:   75 9f 01 mov 159, #1
# CHECK-NEXT:   12 ff f0 lcall 65520
