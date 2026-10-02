; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s

define i16 @increment_word(i16 %value) {
entry:
  %next = add i16 %value, 1
  ret i16 %next
}

define i16 @add_word_offset_0101(i16 %value) {
entry:
  %next = add i16 %value, 257
  ret i16 %next
}

define i16 @add_word_offset_0200(i16 %value) {
entry:
  %next = add i16 %value, 512
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

; CHECK-LABEL: add_word_offset_0101:
; CHECK: inc 131
; CHECK: inc dptr
; CHECK-NOT: add a,
; CHECK: ret

; CHECK-LABEL: add_word_offset_0200:
; CHECK: inc 131
; CHECK: inc 131
; CHECK-NOT: add a,
; CHECK: ret

; CHECK-LABEL: add_words:
; CHECK: add a, 130
; CHECK: mov 130, a
; CHECK: addc a, 131
; CHECK: mov 131, a
; CHECK: ret

; CHECK-LABEL: subtract_words:
; CHECK: clr c
; CHECK: subb a, 130
; CHECK: mov 130, a
; CHECK: subb a, 131
; CHECK: mov 131, a
; CHECK: ret
