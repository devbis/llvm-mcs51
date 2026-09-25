; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s
; RUN: llc -O2 -mtriple=mcs51 -o - %s | FileCheck %s --check-prefix=OPT
; RUN: llc -O0 -mtriple=mcs51 -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

define i8 @choose(i8 %condition) {
entry:
  %test = icmp ne i8 %condition, 0
  br i1 %test, label %yes, label %no

yes:
  ret i8 1

no:
  ret i8 2
}

define i8 @choose_const(i8 %value) {
entry:
  %test = icmp ne i8 %value, 5
  br i1 %test, label %yes, label %no

yes:
  ret i8 1

no:
  ret i8 2
}

define i8 @choose_pair(i8 %lhs, i8 %rhs) {
entry:
  %test = icmp eq i8 %lhs, %rhs
  br i1 %test, label %yes, label %no

yes:
  ret i8 1

no:
  ret i8 2
}

define i8 @choose_signed_less(i8 %lhs, i8 %rhs) {
entry:
  %test = icmp slt i8 %lhs, %rhs
  br i1 %test, label %yes, label %no

yes:
  ret i8 1

no:
  ret i8 2
}

; CHECK-LABEL: choose:
; CHECK: mov a, r7
; CHECK: j{{n?z}}
; CHECK: ljmp
; CHECK: mov a, #1
; CHECK: ret
; CHECK: mov a, #2
; CHECK: ret

; OPT-LABEL: choose:
; OPT: mov a, r7
; OPT: jz
; OPT-NOT: ljmp
; OPT: mov a, #1
; OPT: ret

; DIS-LABEL: <choose>:
; DIS: mov a, r7
; DIS: jz
; DIS: ljmp

; CHECK-LABEL: choose_const:
; CHECK: mov a, r7
; CHECK: xrl a, #5
; CHECK: j{{n?z}}

; CHECK-LABEL: choose_pair:
; CHECK: mov a, r7
; CHECK: xrl a, r6
; CHECK: j{{n?z}}

; CHECK-LABEL: choose_signed_less:
; CHECK: xrl a, #128
; CHECK: xrl a, #128
; CHECK: subb a,
; CHECK: jnc
