; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s

define i8 @divide(i8 %lhs, i8 %rhs) {
entry:
  %quotient = udiv i8 %lhs, %rhs
  ret i8 %quotient
}

define i8 @remainder(i8 %lhs, i8 %rhs) {
entry:
  %rem = urem i8 %lhs, %rhs
  ret i8 %rem
}

; CHECK-LABEL: divide:
; CHECK: mov a, r7
; CHECK: mov 240, r6
; CHECK: div ab
; CHECK: ret

; CHECK-LABEL: remainder:
; CHECK: mov a, r7
; CHECK: mov 240, r6
; CHECK: div ab
; CHECK: mov a, 240
; CHECK: ret
