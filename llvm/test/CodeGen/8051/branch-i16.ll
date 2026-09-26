; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s

declare void @sink()

define void @branch_signed_less(i16 %lhs, i16 %rhs) {
entry:
  %cond = icmp slt i16 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_unsigned_greater_equal(i16 %lhs, i16 %rhs) {
entry:
  %cond = icmp uge i16 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

; CHECK-LABEL: branch_signed_less:
; CHECK: xch a, 240
; CHECK: subb a, 240
; CHECK: jz
; CHECK-LABEL: branch_unsigned_greater_equal:
; CHECK: subb a, 131
; CHECK: jnz
