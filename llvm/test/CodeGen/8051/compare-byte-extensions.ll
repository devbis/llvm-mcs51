; RUN: llc -mtriple=mcs51 -O0 -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=CHECK
; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=OPT

define i8 @unsigned_char_greater(i8 %lhs, i8 %rhs) {
entry:
  %lhs.wide = zext i8 %lhs to i16
  %rhs.wide = zext i8 %rhs to i16
  %cmp = icmp sgt i16 %lhs.wide, %rhs.wide
  %result = zext i1 %cmp to i8
  ret i8 %result
}

define i8 @signed_char_less(i8 %lhs, i8 %rhs) {
entry:
  %lhs.wide = sext i8 %lhs to i16
  %rhs.wide = sext i8 %rhs to i16
  %cmp = icmp slt i16 %lhs.wide, %rhs.wide
  %result = zext i1 %cmp to i8
  ret i8 %result
}

define i8 @unsigned_char_equal(i8 %lhs, i8 %rhs) {
entry:
  %lhs.wide = zext i8 %lhs to i16
  %rhs.wide = zext i8 %rhs to i16
  %cmp = icmp eq i16 %lhs.wide, %rhs.wide
  %result = zext i1 %cmp to i8
  ret i8 %result
}

define i8 @unsigned_char_greater_from_stack(i8 %lhs, i8 %rhs) {
entry:
  %lhs.addr = alloca i8
  %rhs.addr = alloca i8
  store i8 %lhs, ptr %lhs.addr
  store i8 %rhs, ptr %rhs.addr
  %lhs.byte = load i8, ptr %lhs.addr
  %rhs.byte = load i8, ptr %rhs.addr
  %lhs.wide = zext i8 %lhs.byte to i16
  %rhs.wide = zext i8 %rhs.byte to i16
  %cmp = icmp sgt i16 %lhs.wide, %rhs.wide
  %result = zext i1 %cmp to i8
  ret i8 %result
}

; CHECK-LABEL: unsigned_char_greater:
; CHECK: clr c
; CHECK: subb a,
; CHECK: rlc a
; CHECK-LABEL: signed_char_less:
; CHECK: xrl a, #128
; CHECK: xrl a, #128
; CHECK: subb a, 240
; CHECK: rlc a
; CHECK-LABEL: unsigned_char_equal:
; CHECK: xrl a,
; CHECK: jnz
; CHECK: mov a, #1
; CHECK: clr a
; CHECK-LABEL: unsigned_char_greater_from_stack:
; CHECK: subb a,
; CHECK: rlc a

; OPT-LABEL: unsigned_char_greater:
; OPT: subb a,
; OPT: rlc a
; OPT-LABEL: signed_char_less:
; OPT: xrl a, #128
; OPT: xrl a, #128
; OPT: subb a, 240
; OPT-LABEL: unsigned_char_equal:
; OPT: xrl a,
; OPT: jnz
; OPT-LABEL: unsigned_char_greater_from_stack:
; OPT: subb a,
; OPT: rlc a
