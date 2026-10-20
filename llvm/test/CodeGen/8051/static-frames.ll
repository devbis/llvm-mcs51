; -mcs51-static-frames moves frame objects that are only reached by spill and
; frame load/store pseudos into a per-function PDATA object __sframe.<fn>,
; accessed with MOV R1,#__sframe.<fn>+k and MOVX through A. Functions that may
; have two live activations visible to the compiler (direct recursion,
; interrupt handlers, main) and objects whose address is materialised keep
; their hardware stack frame.
; RUN: llc -mtriple=mcs51 -mcs51-csr=none -mcs51-static-frames < %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -mcs51-csr=none < %s | FileCheck %s --check-prefix=OFF

target datalayout = "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-p8:32:8-i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16"

declare i8 @h(i8)

; Three spill slots move to a 3-byte static frame; SP is not adjusted. A is
; saved around a reload that would otherwise clobber the live sum.
; CHECK-LABEL: spills:
; CHECK-NOT: 129
; CHECK: mov a, r5
; CHECK-NEXT: mov r1, #__sframe.spills
; CHECK-NEXT: movx @r1, a
; CHECK-NEXT: mov a, r6
; CHECK-NEXT: mov r1, #__sframe.spills+1
; CHECK-NEXT: movx @r1, a
; CHECK-NEXT: lcall h
; CHECK-NEXT: mov r0, a
; CHECK-NEXT: mov r1, #__sframe.spills+2
; CHECK-NEXT: movx @r1, a
; CHECK-NEXT: mov r1, #__sframe.spills+1
; CHECK-NEXT: movx a, @r1
; CHECK-NEXT: mov r7, a
; CHECK-NEXT: lcall h
; CHECK-NEXT: push 224
; CHECK-NEXT: mov r1, #__sframe.spills+2
; CHECK-NEXT: movx a, @r1
; CHECK-NEXT: mov r0, a
; CHECK-NEXT: pop 224
; CHECK-NEXT: add a, r0
; CHECK-NOT: 129
; CHECK: ret
; OFF-LABEL: spills:
; OFF: inc 129
; OFF-NOT: sframe
; OFF: ret
define internal i8 @spills(i8 %a, i8 %b, i8 %c) {
  %x = call i8 @h(i8 %a)
  %y = call i8 @h(i8 %b)
  %s = add i8 %x, %y
  %t = add i8 %s, %c
  ret i8 %t
}

; CHECK-LABEL: recursive:
; CHECK-NOT: sframe
; CHECK: inc 129
; CHECK: ret
define i8 @recursive(i8 %a) {
  %x = call i8 @recursive(i8 %a)
  %y = add i8 %x, %a
  ret i8 %y
}

; The indexed buffer needs an IDATA address and stays on the hardware stack;
; the spilled pointer to it moves.
; CHECK-LABEL: indexed:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #4
; CHECK: mov r1, #__sframe.indexed
; CHECK-NEXT: movx @r1, a
; CHECK: lcall h
; CHECK: mov r1, #__sframe.indexed
; CHECK-NEXT: movx a, @r1
; CHECK: mov a, @r0
; CHECK: ret
define i8 @indexed(i8 %a, i8 %i) {
  %buf = alloca [4 x i8], align 1
  %p = getelementptr inbounds [4 x i8], ptr %buf, i8 0, i8 %i
  store volatile i8 %a, ptr %p
  %x = call i8 @h(i8 %a)
  %v = load volatile i8, ptr %p
  %y = add i8 %x, %v
  ret i8 %y
}

; CHECK-LABEL: main:
; CHECK-NOT: sframe
; CHECK: inc 129
; CHECK: ret
define i8 @main() {
  %x = call i8 @h(i8 1)
  %y = call i8 @h(i8 2)
  %s = add i8 %x, %y
  ret i8 %s
}

@out = global i8 0

; CHECK-LABEL: isr:
; CHECK-NOT: sframe
; CHECK: inc 129
; CHECK: reti
define void @isr() "interrupt"="1" {
  %x = call i8 @h(i8 1)
  %y = call i8 @h(i8 2)
  %s = add i8 %x, %y
  store volatile i8 %s, ptr @out
  ret void
}

define i8 @caller() {
  %r = call i8 @spills(i8 1, i8 2, i8 3)
  ret i8 %r
}

; CHECK: .section .mcs51.pdata.bss.sframe.spills,"aw",@progbits
; CHECK-NEXT: __sframe.spills:
; CHECK-NEXT: .zero 3
; CHECK: .section .mcs51.pdata.bss.sframe.indexed,"aw",@progbits
; CHECK-NEXT: __sframe.indexed:
; CHECK-NEXT: .zero 1
; CHECK-NOT: __sframe.{{[a-z]+}}:
