; RUN: llc -O2 -mtriple=mcs51 -o - %s | FileCheck %s

@result = addrspace(1) global i8 0
@input = addrspace(1) global i8 0

define void @branch_below_twelve(i8 %value) {
entry:
  %below = icmp ult i8 %value, 12
  br i1 %below, label %yes, label %no

yes:
  store volatile i8 1, ptr addrspace(1) @result
  ret void

no:
  store volatile i8 2, ptr addrspace(1) @result
  ret void
}

define i8 @value_below_twelve(i8 %value) {
entry:
  %below = icmp ult i8 %value, 12
  %result = zext i1 %below to i8
  ret i8 %result
}

define void @loaded_value_below_twelve() {
entry:
  %value = load volatile i8, ptr addrspace(1) @input
  %below = icmp ult i8 %value, 12
  br i1 %below, label %yes, label %no

yes:
  store volatile i8 1, ptr addrspace(1) @result
  ret void

no:
  store volatile i8 2, ptr addrspace(1) @result
  ret void
}

; CHECK-LABEL: branch_below_twelve:
; CHECK: subb a, #12
; CHECK: jnc
; CHECK-LABEL: value_below_twelve:
; CHECK: subb a, #12
; CHECK: rlc a
; CHECK-LABEL: loaded_value_below_twelve:
; CHECK: mov a, input
; CHECK-NOT: mov r0, a
; CHECK: subb a, #12
; CHECK: jnc
