; Indirect calls must keep the argument registers live up to the call. The
; ICALL_W custom inserter used to drop the implicit argument-register uses,
; so every register argument copy before an indirect call was deleted.
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 %s -o - | FileCheck %s

target triple = "mcs51"

@fp2 = external global ptr
@fp5 = external global ptr
@r = global i16 0

; CHECK-LABEL: call_reg_args:
; CHECK: mov r7, a
; CHECK: mov 48, #1
; CHECK: mov 50, #2
; CHECK: mov 130,
; CHECK: mov 131,
; CHECK: lcall .Lcall_reg_args.mcs51.icall
; CHECK: .Lcall_reg_args.mcs51.icall:
; CHECK-NEXT: clr a
; CHECK-NEXT: jmp @a+dptr
define void @call_reg_args() {
  %f = load volatile ptr, ptr @fp2
  %v = call i16 %f(i8 9, i16 2305, i16 2306)
  store volatile i16 %v, ptr @r
  ret void
}

; CHECK-LABEL: call_stack_args:
; CHECK: push
; CHECK: push
; CHECK: mov r7, a
; CHECK: mov 48, #1
; CHECK: mov 50, #2
; CHECK: mov 52, #3
; CHECK: mov 54, #4
; CHECK: lcall .Lcall_stack_args.mcs51.icall
; CHECK-NEXT: dec 129
; CHECK-NEXT: dec 129
define void @call_stack_args() {
  %f = load volatile ptr, ptr @fp5
  %v = call i16 %f(i8 9, i16 2305, i16 2306, i16 2307, i16 2308, i16 39612)
  store volatile i16 %v, ptr @r
  ret void
}
