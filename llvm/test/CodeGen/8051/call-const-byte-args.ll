; Constant byte arguments are materialised in the class of their argument
; register. Selecting them into A kept every constant live in the single A
; register until the call, so all but one were spilled to the hardware stack.
; RUN: llc -mtriple=mcs51 < %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -mcs51-csr=none < %s | FileCheck %s

declare void @b4(i8, i8, i8, i8)
declare void @mixed(i8, i8, i16)

; CHECK-LABEL: c4:
; CHECK-NOT: 129
; CHECK: mov r7, #1
; CHECK-NEXT: mov r6, #2
; CHECK-NEXT: mov r5, #3
; CHECK-NEXT: mov r4, #4
; CHECK-NEXT: lcall b4
define void @c4() {
  call void @b4(i8 1, i8 2, i8 3, i8 4)
  ret void
}

; A non-constant byte argument still goes through its normal copy.
; CHECK-LABEL: cmix:
; CHECK-NOT: 129
; CHECK-DAG: mov r7, #9
; CHECK-DAG: mov 48, #52
; CHECK-DAG: mov 49, #18
; CHECK: lcall mixed
define void @cmix(i8 %x) {
  call void @mixed(i8 9, i8 %x, i16 4660)
  ret void
}
