; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s

define i16 @increment_word(i16 %value) {
entry:
  %next = add i16 %value, 1
  ret i16 %next
}

; CHECK-LABEL: increment_word:
; CHECK: inc dptr
; CHECK: ret
