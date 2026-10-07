; MCS51ISD::BR_EQ/BR_NE are target opcodes. Registering them with
; setTargetDAGCombine indexed past TargetDAGCombineArray when the target
; lowering was constructed, which an assertions build reports for every
; compilation. The combiner still offers target nodes to PerformDAGCombine.
; REQUIRES: asserts
; RUN: llc -mtriple=mcs51 < %s | FileCheck %s

; CHECK-LABEL: branch_eq:
; CHECK: xrl a, #5
; CHECK-NEXT: jnz
; CHECK: ret
define i8 @branch_eq(i8 %x) {
  %c = icmp eq i8 %x, 5
  br i1 %c, label %t, label %f
t:
  ret i8 1
f:
  ret i8 2
}
