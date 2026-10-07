; A byte subtraction whose right operand was just computed in A may swap it
; out of A with XCH. The scan proving A still holds that operand must also
; count a virtual-register def in the A class (here the materialised LHS 0)
; as a clobber; otherwise "clr a" lands between and 0 - (x & 1) became 0.
; RUN: llc -mtriple=mcs51 < %s | FileCheck %s

; CHECK-LABEL: neg_low_bit:
; CHECK: anl a, #1
; CHECK-NEXT: mov [[R:r[0-7]]], a
; CHECK-NOT: xch
; CHECK: clr c
; CHECK-NEXT: subb a, [[R]]
define i8 @neg_low_bit(i8 %x) {
  %b = and i8 %x, 1
  %n = sub i8 0, %b
  ret i8 %n
}

; 8-bit Galois LFSR step: (s >> 1) ^ (s & 1 ? 0xb8 : 0).
; CHECK-LABEL: lfsr:
; CHECK: anl a, #1
; CHECK-NEXT: mov [[M:r[0-7]]], a
; CHECK-NOT: xch
; CHECK: subb a, [[M]]
; CHECK-NEXT: anl a, #-72
define i8 @lfsr(i8 %s) {
  %shr = lshr i8 %s, 1
  %bit = and i8 %s, 1
  %z = icmp eq i8 %bit, 0
  %m = select i1 %z, i8 0, i8 -72
  %r = xor i8 %m, %shr
  ret i8 %r
}
