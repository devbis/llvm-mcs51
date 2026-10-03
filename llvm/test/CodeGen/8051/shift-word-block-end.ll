; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs %s -o /dev/null

; A variable word shift that ends its block must still get a tail block for
; the shift loop, or the PHI of the successor loses its predecessor.

target datalayout = "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-p8:32:8-A2-i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16"
target triple = "mcs51"

declare i8 @ext(i32, i32)

define i32 @f(float %a, float %b) {
entry:
  %c = fcmp ord float %a, %b
  br i1 %c, label %call, label %join

call:
  %r = call signext i8 @ext(i32 1, i32 2)
  %w = sext i8 %r to i32
  br label %join

join:
  %p = phi i32 [ %w, %call ], [ 1, %entry ]
  ret i32 %p
}
