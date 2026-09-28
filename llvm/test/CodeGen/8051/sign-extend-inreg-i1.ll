; RUN: llc -mtriple=mcs51 -verify-machineinstrs < %s | FileCheck %s

target triple = "mcs51"

define i16 @sign_extend_boolean_bit(i16 %value) {
entry:
  %bit = and i16 %value, 1
  %neg = sub i16 0, %bit
  ret i16 %neg
}

; CHECK-LABEL: sign_extend_boolean_bit:
; CHECK: subb a, 130
; CHECK: subb a, 131
; CHECK: ret
