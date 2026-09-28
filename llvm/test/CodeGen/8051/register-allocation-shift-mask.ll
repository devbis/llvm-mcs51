; RUN: llc -O2 -mtriple=mcs51 -o - %s | FileCheck %s

define i8 @mix(i8 %a, i8 %b) {
entry:
  %shifted = shl i8 %a, 2
  %masked = and i8 %b, 3
  %mixed = or i8 %shifted, %masked
  ret i8 %mixed
}

; CHECK-LABEL: mix:
; CHECK: anl a, #3
; CHECK: rlc a
; CHECK: orl a,
; CHECK: ret
