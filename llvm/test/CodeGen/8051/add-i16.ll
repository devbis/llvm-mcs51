; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s

define i16 @increment_word(i16 %value) {
entry:
  %next = add i16 %value, 1
  ret i16 %next
}

define i16 @add_words(i16 %lhs, i16 %rhs) {
entry:
  %sum = add i16 %lhs, %rhs
  ret i16 %sum
}

define i16 @subtract_words(i16 %lhs, i16 %rhs) {
entry:
  %difference = sub i16 %lhs, %rhs
  ret i16 %difference
}

; CHECK-LABEL: increment_word:
; CHECK: inc dptr
; CHECK: ret

; CHECK-LABEL: add_words:
; CHECK: add a,
; CHECK: addc a,
; CHECK: ret

; CHECK-LABEL: subtract_words:
; CHECK: clr c
; CHECK: subb a,
; CHECK: subb a,
; CHECK: ret
