; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -O2 -o - %s | FileCheck %s

target datalayout = "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-p8:32:8-i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16"
target triple = "mcs51"

define i8 @indexed_stack_loop(i8 %seed) #0 {
entry:
  %frame = alloca [12 x i8], align 1
  br label %loop

loop:
  %index = phi i8 [ 0, %entry ], [ %next, %body ]
  %done = icmp eq i8 %index, 12
  br i1 %done, label %exit, label %body

body:
  %offset = zext i8 %index to i16
  %value = add i8 %seed, %index
  %element = getelementptr inbounds i8, ptr %frame, i16 %offset
  store volatile i8 %value, ptr %element, align 1
  %next = add nuw i8 %index, 1
  br label %loop

exit:
  %last = getelementptr inbounds i8, ptr %frame, i16 11
  %result = load volatile i8, ptr %last, align 1
  ret i8 %result
}

attributes #0 = { minsize optsize }

; CHECK-LABEL: indexed_stack_loop:
; CHECK: mov @r
; CHECK: ret
