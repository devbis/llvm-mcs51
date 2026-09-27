; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs -o - %s | FileCheck %s

define i8 @left_shift_byte(i8 %value, i8 %amount) {
entry:
  %shifted = shl i8 %value, %amount
  ret i8 %shifted
}

; CHECK-LABEL: left_shift_byte:
; CHECK: clr c
; CHECK: rlc a
; CHECK: djnz 240

define i8 @left_shift_byte_wide_amount(i8 %value, i16 %amount) {
entry:
  %wide = zext i8 %value to i16
  %shifted = shl i16 %wide, %amount
  %result = trunc i16 %shifted to i8
  ret i8 %result
}

; CHECK-LABEL: left_shift_byte_wide_amount:
; CHECK: mov a, 131
; CHECK: jnz
; CHECK: djnz 240
