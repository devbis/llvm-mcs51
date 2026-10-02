; RUN: llc -mtriple=mcs51 -O2 -filetype=obj %s -o /dev/null
; RUN: llc -mtriple=mcs51 -O2 %s -o - | FileCheck %s

; Inline assembly contributes to branch distances. Without it, branch
; relaxation left a short conditional branch that the assembler rejected.

target datalayout = "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-p8:32:8-A2-i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16"
target triple = "mcs51"

@flag = addrspace(4) global i8 0

; CHECK-LABEL: skip_inline_asm:
; CHECK: ljmp
define void @skip_inline_asm() {
entry:
  %v = load volatile i8, ptr addrspace(4) @flag, align 1
  %c = icmp eq i8 %v, 0
  br i1 %c, label %done, label %body

body:
  call void asm sideeffect "nop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop\0Anop", ""()
  store volatile i8 1, ptr addrspace(4) @flag, align 1
  br label %done

done:
  ret void
}
