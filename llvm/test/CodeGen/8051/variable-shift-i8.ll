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
