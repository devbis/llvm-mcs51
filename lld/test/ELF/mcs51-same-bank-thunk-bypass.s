# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-objdump -d %t.elf | FileCheck %s

# A call or tail jump through a ".direct." trampoline from the callee's own
# bank goes straight to the callee. Calls from common or another bank, callees
# whose trampoline lacks the marker (incoming stack arguments) and function
# addresses keep the trampoline.
.section .vectors,"ax"
.globl reset
reset:
  lcall __mcs51_bankcall_near
  ret

.section .bank1.caller,"ax"
.globl caller
.type caller,@function
caller:
  lcall __mcs51_bankcall_near
  lcall __mcs51_bankcall_far
  lcall __mcs51_bankcall_stackarg
  lcall __mcs51_bankcall_local
  mov dptr, #__mcs51_bankcall_near
  ljmp __mcs51_bankcall_near

.section .bank1.near,"ax"
.globl near
.type near,@function
near:
  ret

.section .bank1.stackarg,"ax"
.globl stackarg
.type stackarg,@function
stackarg:
  ret

.section .bank1.local,"ax"
.type local,@function
local:
  ret

.section .bank2.far,"ax"
.globl far
.type far,@function
far:
  ret

.section .bank2.other,"ax"
.globl other
.type other,@function
other:
  lcall __mcs51_bankcall_near
  ret

.section .text.autobankthunks.direct.0,"ax"
.globl __mcs51_bankcall_near
__mcs51_bankcall_near:
  push 159
  mov 159, #near
  lcall near
  pop 159
  ret

.section .text.autobankthunks.direct.1,"ax"
.globl __mcs51_bankcall_far
__mcs51_bankcall_far:
  push 159
  mov 159, #far
  lcall far
  pop 159
  ret

.section .text.autobankthunks.2,"ax"
.globl __mcs51_bankcall_stackarg
__mcs51_bankcall_stackarg:
  push 159
  mov 159, #stackarg
  lcall stackarg
  pop 159
  ret

.section .text.autobankthunks.direct.3,"ax"
.local __mcs51_bankcall_local
__mcs51_bankcall_local:
  push 159
  mov 159, #local
  lcall local
  pop 159
  ret

# CHECK:      <reset>:
# CHECK-NEXT:   12 00 94     lcall 148
# CHECK:      00000094 <__mcs51_bankcall_near>:
# CHECK-NEXT:   c0 9f        push 159
# CHECK-NEXT:   75 9f 01     mov 159, #1
# CHECK-NEXT:   12 80 12     lcall 32786
# CHECK:      0000009f <__mcs51_bankcall_far>:
# CHECK:      000000aa <__mcs51_bankcall_stackarg>:
# CHECK:      000000b5 <__mcs51_bankcall_local>:
# CHECK:      00008000 <caller>:
# CHECK-NEXT:   12 80 12     lcall 32786
# CHECK-NEXT:   12 00 9f     lcall 159
# CHECK-NEXT:   12 00 aa     lcall 170
# CHECK-NEXT:   12 80 14     lcall 32788
# CHECK-NEXT:   90 00 94     mov dptr, #148
# CHECK-NEXT:   02 80 12     ljmp 32786
# CHECK:      00008012 <near>:
# CHECK:      00008014 <local>:
# CHECK:      00008001 <other>:
# CHECK-NEXT:   12 00 94     lcall 148
