; Call matrix for explicitly banked functions (cc2530). Every call into a bank
; goes through the callee's trampoline:
;   __mcs51_bankcall_F: push FMAP ; mov FMAP,#bank ; lcall F ; pop FMAP ; ret
; so a banked callee finds, from SP downwards at entry:
;   [F ret lo/hi (2)] [saved FMAP (1)] [trampoline ret lo/hi (2)] [args...]
; while a common-flash callee entered by a direct LCALL finds:
;   [F ret lo/hi (2)] [args...]
; Banked callees therefore read their stack arguments MCS51BankThunkFrameBytes
; (3) bytes further below SP. Register arguments and return values pass
; through the trampoline unchanged because it only touches FMAP and SP.
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 %s -o - | FileCheck %s

target triple = "mcs51"

; Five i16 arguments: four in IP0-IP3, the fifth on the stack. Offsets below
; are relative to SP after the callee's one-byte register save (push 2).
; Common callee: the stack word lies just below the return address.
; CHECK-LABEL: root_stack:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #252
define i16 @root_stack(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline {
  ret i16 %e
}

; Bank 1 callee: three bytes further down, below the trampoline frame.
; CHECK-LABEL: bank1_stack:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #249
; CHECK: __mcs51_bankcall_bank1_stack:
; CHECK-NEXT: push 159
; CHECK-NEXT: mov 159, #1
; CHECK-NEXT: lcall bank1_stack
; CHECK-NEXT: pop 159
; CHECK-NEXT: ret
define i16 @bank1_stack(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline section ".bank1.text" {
  ret i16 %e
}

; Mixed i8 register arguments and a stack word, bank 2 callee.
; CHECK-LABEL: bank2_mixed:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #249
define i16 @bank2_mixed(i8 %r0, i8 %r1, i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline section ".bank2.text" {
  %x = zext i8 %r1 to i16
  %y = add i16 %e, %x
  ret i16 %y
}

; Register-only callees.
define i8 @bank1_reg(i8 %x) noinline section ".bank1.text" {
  %y = add i8 %x, 1
  ret i8 %y
}

define i8 @root_reg(i8 %x) noinline {
  %y = add i8 %x, 2
  ret i8 %y
}

; root -> bank, stack arguments: through the trampoline; the i16 result comes
; back in IP0 (48/49).
; CHECK-LABEL: call_root_to_bank_stack:
; CHECK: push 49
; CHECK: push 48
; CHECK: lcall __mcs51_bankcall_bank1_stack
; CHECK: ret
define i16 @call_root_to_bank_stack(i16 %x) noinline {
  %r = call i16 @bank1_stack(i16 1, i16 2, i16 3, i16 4, i16 %x)
  ret i16 %r
}

; root -> bank, register arguments only: still a trampoline call (different
; bank); the i8 result comes back in A.
; CHECK-LABEL: call_root_to_bank_reg:
; CHECK: lcall __mcs51_bankcall_bank1_reg
; CHECK-NEXT: ret
define i8 @call_root_to_bank_reg(i8 %x) noinline {
  %r = call i8 @bank1_reg(i8 %x)
  ret i8 %r
}

; bank -> root, stack arguments: a direct LCALL, matching the common callee's
; 2-byte bias.
; CHECK-LABEL: call_bank_to_root_stack:
; CHECK-NOT: __mcs51_bankcall_root_stack
; CHECK: lcall root_stack
define i16 @call_bank_to_root_stack(i16 %x) noinline section ".bank1.text" {
  %r = call i16 @root_stack(i16 1, i16 2, i16 3, i16 4, i16 %x)
  ret i16 %r
}

; bank -> root, register arguments only: direct.
; CHECK-LABEL: call_bank_to_root_reg:
; CHECK-NOT: __mcs51_bankcall_root_reg
; CHECK: lcall root_reg
define i8 @call_bank_to_root_reg(i8 %x) noinline section ".bank1.text" {
  %r = call i8 @root_reg(i8 %x)
  ret i8 %r
}

; bank 1 -> bank 2, mixed register and stack arguments: through the trampoline.
; CHECK-LABEL: call_bank_to_bank_mixed:
; CHECK: push 49
; CHECK: push 48
; CHECK: lcall __mcs51_bankcall_bank2_mixed
define i16 @call_bank_to_bank_mixed(i8 %v, i16 %x) noinline section ".bank1.text" {
  %r = call i16 @bank2_mixed(i8 7, i8 %v, i16 1, i16 2, i16 3, i16 4, i16 %x)
  ret i16 %r
}

; Same bank, stack arguments: the trampoline, so the callee's 3-byte bias
; holds for every caller.
; CHECK-LABEL: call_same_bank_stack:
; CHECK: lcall __mcs51_bankcall_bank1_stack
define i16 @call_same_bank_stack(i16 %x) noinline section ".bank1.text" {
  %r = call i16 @bank1_stack(i16 1, i16 2, i16 3, i16 4, i16 %x)
  ret i16 %r
}

; Same bank, register arguments only: a direct LCALL is still valid.
; CHECK-LABEL: call_same_bank_reg:
; CHECK-NOT: __mcs51_bankcall_
; CHECK: lcall bank1_reg
define i8 @call_same_bank_reg(i8 %x) noinline section ".bank1.text" {
  %r = call i8 @bank1_reg(i8 %x)
  ret i8 %r
}

