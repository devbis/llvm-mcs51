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
; CHECK: inc 48
; CHECK: mov a, 48
; CHECK: jnz
; CHECK: inc 49
; CHECK: ret

; CHECK-LABEL: add_word_offset_0101:
; CHECK: mov a, 48
; CHECK: add a, #1
; CHECK: mov 48, a
; CHECK: mov a, 49
; CHECK: addc a, #1
; CHECK: mov 49, a
; CHECK: ret

; CHECK-LABEL: add_word_offset_0200:
; CHECK-NOT: add a, #0
; CHECK: mov a, 49
; CHECK: add a, #2
; CHECK: mov 49, a
; CHECK-NOT: addc
; CHECK: ret

; CHECK-LABEL: add_words:
; CHECK: mov a, 48
; CHECK: add a, 50
; CHECK: mov 48, a
; CHECK: mov a, 49
; CHECK: addc a, 51
; CHECK: mov 49, a
; CHECK: ret

; CHECK-LABEL: subtract_words:
; CHECK: clr c
; CHECK: mov a, 48
; CHECK: subb a, 50
; CHECK: mov 48, a
; CHECK: mov a, 49
; CHECK: subb a, 51
; CHECK: mov 49, a
; CHECK: ret
