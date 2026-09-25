; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s
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

; CHECK-LABEL: choose:
; CHECK: mov a, r7
; CHECK: jz
; CHECK: ljmp
; CHECK: mov a, #1
; CHECK: ret
; CHECK: mov a, #2
; CHECK: ret

; DIS-LABEL: <choose>:
; DIS: mov a, r7
; DIS: jz
; DIS: ljmp
