; -mcs51-csr selects the callee-saved register set. The default keeps R2-R7
; and IM8-IM23; "r" keeps only R2-R7 and "none" makes every register
; caller-saved, so values live across a call are spilled by the caller.
; RUN: llc -mtriple=mcs51 < %s | FileCheck %s --check-prefixes=CHECK,DEFAULT
; RUN: llc -mtriple=mcs51 -mcs51-csr=default < %s | FileCheck %s --check-prefixes=CHECK,DEFAULT
; RUN: llc -mtriple=mcs51 -mcs51-csr=r < %s | FileCheck %s --check-prefixes=CHECK,R
; RUN: llc -mtriple=mcs51 -mcs51-csr=none < %s | FileCheck %s --check-prefixes=CHECK,NONE
; RUN: not llc -mtriple=mcs51 -mcs51-csr=bogus < %s 2>&1 | FileCheck %s --check-prefix=BAD
; BAD: Cannot find option named 'bogus'

declare void @g()
declare i8 @h(i8)

; R7 is callee-saved unless the set is empty.
; CHECK-LABEL: keep_byte:
; DEFAULT-NOT: 129
; DEFAULT: lcall g
; DEFAULT-NEXT: mov a, r7
; R-NOT: 129
; R: lcall g
; R-NEXT: mov a, r7
; NONE: inc 129
; NONE-NEXT: mov r1, 129
; NONE-NEXT: mov @r1, r7
; NONE-NEXT: lcall g
; NONE-NEXT: mov r1, 129
; NONE-NEXT: mov r0, @r1
; NONE-NEXT: dec 129
; CHECK: ret
define i8 @keep_byte(i8 %x) {
  call void @g()
  ret i8 %x
}

; A word live across the call is kept in IM8/IM9 (56/57) only by the default
; set; otherwise it goes to a stack slot. R7 (direct address 7) is saved by
; the callee under the default and "r" sets.
; CHECK-LABEL: keep_many:
; DEFAULT: push 7
; DEFAULT-NEXT: push 56
; DEFAULT-NEXT: push 57
; R: push 7
; R-NOT: push
; NONE-NOT: push
; CHECK: lcall h
; DEFAULT: pop 57
; DEFAULT-NEXT: pop 56
; DEFAULT-NEXT: pop 7
; R-NOT: pop 5
; R: pop 7
; NONE-NOT: pop
; CHECK: ret
define i16 @keep_many(i16 %a, i16 %b, i16 %c) {
  %x = call i8 @h(i8 1)
  %s = add i16 %a, %b
  %t = add i16 %s, %c
  %e = zext i8 %x to i16
  %r = add i16 %t, %e
  ret i16 %r
}

; Indirect calls carry the same call-preserved mask.
; CHECK-LABEL: keep_icall:
; DEFAULT-NOT: 129
; NONE: mov @r1, r7
; CHECK: lcall .Lkeep_icall.mcs51.icall
; DEFAULT-NEXT: mov a, r7
; NONE-NEXT: mov r1, 129
; NONE-NEXT: mov r0, @r1
; CHECK: ret
define i8 @keep_icall(ptr %f, i8 %x) {
  call void %f()
  ret i8 %x
}

; An interrupt handler saves every register its callees may clobber. With a
; reduced set the call's register mask is the only record that R2-R7 (and
; IM8-IM23) are clobbered, so the save set must be derived from the mask.
; CHECK-LABEL: isr_call:
; CHECK: push 1{{$}}
; DEFAULT-NEXT: push 48
; R-NEXT: push 48
; NONE-NEXT: push 2
; NONE-NEXT: push 3
; NONE-NEXT: push 4
; NONE-NEXT: push 5
; NONE-NEXT: push 6
; NONE-NEXT: push 7{{$}}
; NONE-NEXT: push 48
; CHECK: push 55
; DEFAULT-NEXT: lcall g
; R-NEXT: push 56
; R: push 71
; R-NEXT: lcall g
; NONE-NEXT: push 56
; NONE: push 71
; NONE-NEXT: lcall g
; CHECK: pop 0{{$}}
; CHECK: reti
define void @isr_call() "interrupt"="0" {
  call void @g()
  ret void
}
