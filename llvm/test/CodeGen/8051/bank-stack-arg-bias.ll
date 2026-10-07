; A banked function is entered through its bank-call trampoline
; (push FMAP; mov FMAP,#bank; lcall F), which leaves three bytes between the
; caller's outgoing stack arguments and the callee's return address. Banked
; callees must read incoming stack arguments three bytes further below SP than
; common callees, and every call to a banked function that passes stack
; arguments must go through the trampoline so the layout matches.
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 %s -o - | FileCheck %s
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -function-sections %s -o - \
; RUN:   | FileCheck %s --check-prefix=AUTO

target triple = "mcs51"

; Common (non-banked) callee: return address only.
; CHECK-LABEL: common_arg5:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #252
; AUTO-LABEL: common_arg5:
; AUTO: mov a, 129
; AUTO-NEXT: add a, #249
define i16 @common_arg5(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline {
  ret i16 %e
}

; Runtime helpers stay in common flash even with function sections.
; AUTO-LABEL: __mcs51_helper_arg5:
; AUTO: mov a, 129
; AUTO-NEXT: add a, #252
define i16 @__mcs51_helper_arg5(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline {
  ret i16 %e
}

; Explicitly banked callee: return address + trampoline frame.
; CHECK-LABEL: explicit_arg5:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #249
define i16 @explicit_arg5(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline section ".bank1.text" {
  ret i16 %e
}

; A same-bank call that passes stack arguments uses the trampoline.
; CHECK-LABEL: explicit_same_bank_stack:
; CHECK: push 49
; CHECK: push 48
; CHECK: lcall __mcs51_bankcall_explicit_arg5
define i16 @explicit_same_bank_stack(i16 %x) noinline section ".bank1.text" {
  %r = call i16 @explicit_arg5(i16 1, i16 2, i16 3, i16 4, i16 %x)
  ret i16 %r
}

define i8 @explicit_reg_only(i8 %x) noinline section ".bank1.text" {
  ret i8 %x
}

; A same-bank call with register arguments only remains a direct LCALL.
; CHECK-LABEL: explicit_same_bank_reg:
; CHECK-NOT: __mcs51_bankcall_
; CHECK: lcall explicit_reg_only
; CHECK-NEXT: ret
define i8 @explicit_same_bank_reg(i8 %x) noinline section ".bank1.text" {
  %r = call i8 @explicit_reg_only(i8 %x)
  ret i8 %r
}

; The program entry point is never auto-banked. main saves no callee-saved
; registers, so the unbiased offset is SP-3.
; AUTO-LABEL: main:
; AUTO: mov a, 129
; AUTO-NEXT: add a, #253
define i16 @main(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) noinline {
  ret i16 %e
}
