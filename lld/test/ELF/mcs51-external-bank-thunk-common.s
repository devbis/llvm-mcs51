# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/../../../llvm/lib/Target/MCS51/cc2530.ld \
# RUN:   --no-check-sections -o %t.elf %t.o
# RUN: llvm-objdump -d %t.elf | FileCheck %s

# A caller that only sees a declaration emits a weak external trampoline in
# .text.autobankthunks.external.*. When the callee ends up in common flash the
# linker must bind the call straight to the callee: common functions are always
# entered directly, and their incoming stack arguments sit right below the
# return address. A callee placed in a bank keeps the trampoline.
.section .vectors,"ax"
.globl reset
reset:
  lcall __mcs51_bankcall_common_fn
  lcall __mcs51_bankcall_banked_fn
  ret

.section .mcs51.common.text,"ax"
.globl common_fn
.type common_fn,@function
common_fn:
  ret

.section .mcs51.autobank.banked_fn,"ax"
.globl banked_fn
.type banked_fn,@function
banked_fn:
  ret

.section .text.autobankthunks.external.0,"ax"
.weak __mcs51_bankcall_common_fn
__mcs51_bankcall_common_fn:
  push 159
  mov 159, #common_fn
  lcall common_fn
  pop 159
  ret

.section .text.autobankthunks.external.1,"ax"
.weak __mcs51_bankcall_banked_fn
__mcs51_bankcall_banked_fn:
  push 159
  mov 159, #banked_fn
  lcall banked_fn
  pop 159
  ret

# CHECK:      <reset>:
# CHECK-NEXT:   12 00 94     lcall 148
# CHECK-NEXT:   12 00 a3     lcall 163
# CHECK:      00000094 <common_fn>:
# CHECK:      000000a3 <__mcs51_bankcall_banked_fn>:
# CHECK-NEXT:   c0 9f        push 159
# CHECK-NEXT:   75 9f 01     mov 159, #1
# CHECK-NEXT:   12 80 00     lcall 32768
