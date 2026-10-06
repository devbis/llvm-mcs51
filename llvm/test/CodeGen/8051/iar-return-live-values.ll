; RUN: llc -O2 -mtriple=mcs51 -mcpu=cc2530 %s -o - | FileCheck %s

target triple = "mcs51"

define i16 @iar_word_sum(i16 %a, i16 %b) #0 {
  %sum = add i16 %a, %b
  ret i16 %sum
}

define i8 @iar_byte_increment(i8 %value) #0 {
  %result = add i8 %value, 1
  ret i8 %result
}

attributes #0 = { "mcs51-abi"="iar" }

; Optimized returns must keep IAR's result registers live through RET.
; CHECK-LABEL: iar_word_sum:
; CHECK: mov r2,
; CHECK: mov r3,
; CHECK: ret
; CHECK-LABEL: iar_byte_increment:
; CHECK: inc r1
; CHECK-NEXT: ret
