; RUN: llc -mtriple=mcs51 -verify-machineinstrs < %s | FileCheck %s

define i1 @compare_in_predecessor(i1 %take, i16 %lhs, i16 %rhs) {
entry:
  br i1 %take, label %compare, label %merge

compare:
  %equal = icmp eq i16 %lhs, %rhs
  br label %merge

merge:
  %result = phi i1 [ %equal, %compare ], [ false, %entry ]
  ret i1 %result
}

; CHECK-LABEL: compare_in_predecessor:
; The compare result must be combined with %take (r7) without being lost.
; CHECK:       xrl a, 50
; CHECK:       xrl a, 51
; CHECK:       orl a,
; CHECK:       anl a, r7
; CHECK:       ret
