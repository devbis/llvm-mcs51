; Under minsize, i8 udiv/urem select DIV AB. The remainder is read from B;
; that read must be a modeled use of B, otherwise DIV AB (whose A and B
; results then look dead) is deleted and urem returned the divisor.
; RUN: llc -mtriple=mcs51 < %s | FileCheck %s

; CHECK-LABEL: rem_const:
; CHECK: mov a, r7
; CHECK: mov 240, r{{[0-7]}}
; CHECK-NEXT: div ab
; CHECK-NEXT: mov a, 240
; CHECK: ret
define i8 @rem_const(i8 %x) minsize optsize {
  %v = urem i8 %x, 41
  ret i8 %v
}

; CHECK-LABEL: rem_var:
; CHECK: div ab
; CHECK-NEXT: mov a, 240
; CHECK: ret
define i8 @rem_var(i8 %x, i8 %y) minsize optsize {
  %v = urem i8 %x, %y
  ret i8 %v
}

; CHECK-LABEL: div_const:
; CHECK: div ab
; CHECK-NOT: mov a, 240
; CHECK: ret
define i8 @div_const(i8 %x) minsize optsize {
  %v = udiv i8 %x, 41
  ret i8 %v
}
