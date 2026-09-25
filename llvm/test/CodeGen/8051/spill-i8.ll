; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s

define i8 @sum_ten(i8 %a, i8 %b, i8 %c, i8 %d, i8 %e, i8 %f, i8 %g,
                   i8 %h, i8 %i, i8 %j) {
entry:
  %ab = add i8 %a, %b
  %abc = add i8 %ab, %c
  %abcd = add i8 %abc, %d
  %abcde = add i8 %abcd, %e
  %abcdef = add i8 %abcde, %f
  %abcdefg = add i8 %abcdef, %g
  %abcdefgh = add i8 %abcdefg, %h
  %abcdefghi = add i8 %abcdefgh, %i
  %abcdefghij = add i8 %abcdefghi, %j
  ret i8 %abcdefghij
}

; CHECK-LABEL: sum_ten:
; CHECK: inc 129
; CHECK: mov @r1, a
; CHECK: dec 129
; CHECK: ret
