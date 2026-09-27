; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s

declare void @sink()

define void @far_conditional(i8 %condition) {
entry:
  %test = icmp ne i8 %condition, 0
  br i1 %test, label %far, label %filler

filler:
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  call void @sink()
  br label %far

far:
  ret void
}

; CHECK-LABEL: far_conditional:
; CHECK: jz
; CHECK: ljmp
; CHECK: lcall sink
