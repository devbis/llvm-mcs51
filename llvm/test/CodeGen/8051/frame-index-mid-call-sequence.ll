; RUN: llc -mtriple=mcs51 -O2 %s -o - | FileCheck %s

; The i64 abs/select expansions are custom-inserted control flow that lands
; between ADJCALLSTACKDOWN and the call, after part of the arguments has been
; pushed. Every block that the inserters create must record the enclosing
; call frame size (machine verifier), and a frame index used in such a block
; must be resolved against the bytes pushed so far instead of aborting with
; "frame index in a block that starts inside a call sequence".

declare i64 @callee(i64, i64, ptr)

define i64 @caller(i64 %n, i64 %d) minsize optsize {
; CHECK-LABEL: caller:
; CHECK:       add a, #245
; CHECK-NEXT:  mov r1, a
; CHECK:       push 54
; CHECK-NEXT:  lcall callee
entry:
  %r = alloca i64, align 1, addrspace(2)
  %g = addrspacecast ptr addrspace(2) %r to ptr
  %a = call i64 @llvm.abs.i64(i64 %n, i1 false)
  %b = call i64 @llvm.abs.i64(i64 %d, i1 false)
  %c = call i64 @callee(i64 %a, i64 %b, ptr %g)
  %x = xor i64 %d, %n
  %neg = sub i64 0, %c
  %sign = icmp slt i64 %x, 0
  %q = select i1 %sign, i64 %neg, i64 %c
  ret i64 %q
}
