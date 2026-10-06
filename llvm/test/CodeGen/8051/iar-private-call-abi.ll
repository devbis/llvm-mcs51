; RUN: llc -O2 -mtriple=mcs51 -mcpu=cc2530 %s -o - | FileCheck %s

target triple = "mcs51"

define internal i16 @private_add(i16 %value) #0 {
  %result = add i16 %value, 1
  ret i16 %result
}

define i16 @public_call_private(i16 %value, i16 %live) #0 {
  %result = call i16 @private_add(i16 %value)
  %sum = add i16 %result, %live
  ret i16 %sum
}

declare i8 @iar_external(i8) #0

define i8 @public_call_iar(i8 %value) #0 {
  %result = call i8 @iar_external(i8 %value)
  ret i8 %result
}

attributes #0 = { "mcs51-abi"="iar" }

; The private callee uses Clang's convention. The public caller preserves its
; second argument across that call and returns the result through IAR's R2:R3.
; The external call keeps the IAR convention.
; CHECK-LABEL: private_add:
; CHECK: inc 48
; CHECK-LABEL: public_call_private:
; CHECK: lcall private_add
; CHECK: add a,
; CHECK: addc a,
; CHECK: mov r2,
; CHECK: mov r3,
; CHECK-LABEL: public_call_iar:
; CHECK: lcall iar_external
; CHECK-NEXT: ret
