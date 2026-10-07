; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs %s -o - | FileCheck %s

; The post-RA peephole caches "R1 holds SP+k" to rewrite stack addressing as
; relative R1 adjustments. Argument pushes move SP, so the cache must be
; invalidated by every SP write; otherwise the out-of-line pointer argument
; and the stack-passed i32 arguments are addressed with a stale SP and the
; callee receives the wrong bytes.

declare i32 @callee32(i32, i32, i32, i32, ptr)

define i32 @caller32(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: caller32:
; CHECK:       mov 56, r2
; CHECK-NEXT:  mov a, 129
; CHECK-NEXT:  add a, #{{[0-9]+}}
; CHECK-NEXT:  mov r1, a
; CHECK:       push 56
; CHECK-NEXT:  mov a, 129
; CHECK-NEXT:  add a, #245
; CHECK-NEXT:  mov r1, a
; CHECK-NOT:   push
; CHECK:       lcall callee32
entry:
  %r = alloca i32, align 1, addrspace(2)
  %r.ascast = addrspacecast ptr addrspace(2) %r to ptr
  %call = call i32 @callee32(i32 %a, i32 %b, i32 %c, i32 %d, ptr %r.ascast)
  %0 = load i32, ptr %r.ascast, align 1
  %xor = xor i32 %0, %call
  ret i32 %xor
}
