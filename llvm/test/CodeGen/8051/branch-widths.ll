; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -verify-machineinstrs -o /dev/null %s

declare void @sink()

define i8 @equal_i16(i16 %lhs, i16 %rhs) {
entry:
  %cond = icmp eq i16 %lhs, %rhs
  %result = zext i1 %cond to i8
  ret i8 %result
}

define void @branch_not_equal_i16(i16 %lhs, i16 %rhs) {
entry:
  %cond = icmp ne i16 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

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

define void @branch_equal_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp eq i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_signed_less_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp slt i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_unsigned_greater_equal_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp uge i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_not_equal_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp ne i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_signed_greater_equal_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp sge i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_signed_greater_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp sgt i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_signed_less_equal_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp sle i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_unsigned_less_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp ult i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_unsigned_greater_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp ugt i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_unsigned_less_equal_i32(i32 %lhs, i32 %rhs) {
entry:
  %cond = icmp ule i32 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_equal_i64(i64 %lhs, i64 %rhs) {
entry:
  %cond = icmp eq i64 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_signed_less_i64(i64 %lhs, i64 %rhs) {
entry:
  %cond = icmp slt i64 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

define void @branch_unsigned_greater_equal_i64(i64 %lhs, i64 %rhs) {
entry:
  %cond = icmp uge i64 %lhs, %rhs
  br i1 %cond, label %taken, label %exit
taken:
  call void @sink()
  br label %exit
exit:
  ret void
}

; CHECK-LABEL: equal_i16:
; CHECK: xrl a,
; CHECK: jnz
; CHECK: mov a, #1
; CHECK: mov a, #0

; CHECK-LABEL: branch_not_equal_i16:
; CHECK: xrl a,
; CHECK: jnz

; CHECK-LABEL: branch_signed_less:
; CHECK: xch a, 240
; CHECK: subb a, 240
; CHECK: jz
; CHECK-LABEL: branch_unsigned_greater_equal:
; CHECK: subb a, 131
; CHECK: jnz

; CHECK-LABEL: branch_equal_i32:
; CHECK: jnz
; CHECK-LABEL: branch_signed_less_i32:
; CHECK: jnz
; CHECK-LABEL: branch_unsigned_greater_equal_i32:
; CHECK: jnz
; CHECK-LABEL: branch_not_equal_i32:
; CHECK-LABEL: branch_signed_greater_equal_i32:
; CHECK-LABEL: branch_signed_greater_i32:
; CHECK-LABEL: branch_signed_less_equal_i32:
; CHECK-LABEL: branch_unsigned_less_i32:
; CHECK-LABEL: branch_unsigned_greater_i32:
; CHECK-LABEL: branch_unsigned_less_equal_i32:
; CHECK-LABEL: branch_equal_i64:
; CHECK-LABEL: branch_signed_less_i64:
; CHECK-LABEL: branch_unsigned_greater_equal_i64:
