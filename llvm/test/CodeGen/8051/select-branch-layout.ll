; RUN: llc -O2 -mtriple=mcs51 -o - %s | FileCheck %s

@x = addrspace(1) global i8 0
@result = addrspace(1) global i8 0

define void @select_after_branch() {
entry:
  store volatile i8 7, ptr addrspace(1) @x
  %first = load volatile i8, ptr addrspace(1) @x
  %below_four = icmp ult i8 %first, 4
  br i1 %below_four, label %small, label %other

small:
  br label %join

other:
  %second = load volatile i8, ptr addrspace(1) @x
  %below_ten = icmp ult i8 %second, 10
  %selected = select i1 %below_ten, i8 2, i8 3
  br label %join

join:
  %value = phi i8 [ 1, %small ], [ %selected, %other ]
  store volatile i8 %value, ptr addrspace(1) @result
  ret void
}

; CHECK-LABEL: select_after_branch:
; CHECK: jnc
; CHECK: mov a, #1
; CHECK: jz
; CHECK: mov a, #2
; CHECK: mov a, #3
; CHECK: mov result, a
; CHECK: ret
