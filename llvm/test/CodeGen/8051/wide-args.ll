; -mcs51-wide-args passes words 5-8 in IP4-IP7 (IM8-IM15) and bytes 5-12 in
; IM16-IM23 before using the hardware stack.
; RUN: llc -mtriple=mcs51 -mcs51-csr=none -mcs51-wide-args < %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -mcs51-csr=r -mcs51-wide-args < %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -mcs51-csr=none < %s | FileCheck %s --check-prefix=NARROW
; IM8-IM23 are callee-saved under the default set, so the option is refused.
; RUN: not --crash llc -mtriple=mcs51 -mcs51-wide-args < %s 2>&1 | FileCheck %s --check-prefix=REFUSE
; REFUSE: -mcs51-wide-args requires -mcs51-csr=r or none

declare void @w9(i16, i16, i16, i16, i16, i16, i16, i16, i16)
declare void @b12(i8, i8, i8, i8, i8, i8, i8, i8, i8, i8, i8, i8)
declare void @b13(i8, i8, i8, i8, i8, i8, i8, i8, i8, i8, i8, i8, i8)
declare void @va(i16, ...)

; The ninth word is the only stack argument.
; CHECK-LABEL: cw:
; CHECK: push 49
; CHECK-NEXT: push 48
; CHECK-NOT: push
; CHECK-DAG: mov 62, #8
; CHECK-DAG: mov 56, #5
; CHECK: lcall w9
; CHECK-NEXT: dec 129
; CHECK-NEXT: dec 129
; NARROW-LABEL: cw:
; NARROW-COUNT-10: push
; NARROW: lcall w9
define void @cw() {
  call void @w9(i16 1, i16 2, i16 3, i16 4, i16 5, i16 6, i16 7, i16 8, i16 9)
  ret void
}

; CHECK-LABEL: cb:
; CHECK-NOT: push
; CHECK-NOT: 129
; CHECK-DAG: mov r4, #4
; CHECK-DAG: mov 64, #5
; CHECK-DAG: mov 71, #12
; CHECK: lcall b12
; CHECK-NEXT: ret
define void @cb() {
  call void @b12(i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7, i8 8, i8 9, i8 10, i8 11, i8 12)
  ret void
}

; The thirteenth byte is pushed.
; CHECK-LABEL: cb13:
; CHECK: push
; CHECK-NOT: push
; CHECK: lcall b13
; CHECK-NEXT: dec 129
define void @cb13() {
  call void @b13(i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7, i8 8, i8 9, i8 10, i8 11, i8 12, i8 13)
  ret void
}

; Variadic arguments stay on the stack.
; CHECK-LABEL: cva:
; CHECK: push
; CHECK: push
; CHECK: lcall va
define void @cva() {
  call void (i16, ...) @va(i16 1, i16 2)
  ret void
}

; Callee side: the fifth and eighth words are IP4 (56,57) and IP7 (62,63).
; CHECK-LABEL: rw:
; CHECK: mov a, 56
; CHECK-NEXT: add a, 62
; CHECK: mov a, 57
; CHECK-NEXT: addc a, 63
define i16 @rw(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e, i16 %f, i16 %g, i16 %h) {
  %s = add i16 %e, %h
  ret i16 %s
}

; The fifth and twelfth bytes are IM16 (64) and IM23 (71).
; CHECK-LABEL: rb:
; CHECK-DAG: 64
; CHECK-DAG: 71
; CHECK: ret
define i8 @rb(i8 %a, i8 %b, i8 %c, i8 %d, i8 %e, i8 %f, i8 %g, i8 %h, i8 %i, i8 %j, i8 %k, i8 %l) {
  %s = add i8 %e, %l
  ret i8 %s
}
