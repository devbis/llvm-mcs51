; Generic pointer accesses of integer widths without a runtime helper (i24,
; i40, i48, i56) are split into supported chunks at increasing byte offsets.
; i24 values arise from 3-byte structures returned or copied as integers.
; RUN: llc -mtriple=mcs51 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -print-after=inline-asm-prepare -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=LOWERED

define i24 @load_i24(ptr addrspace(8) %p) {
  %v = load i24, ptr addrspace(8) %p, align 1
  ret i24 %v
}

define void @store_i24(ptr addrspace(8) %p, i24 %v) {
  store i24 %v, ptr addrspace(8) %p, align 1
  ret void
}

define i48 @load_i48(ptr addrspace(8) %p) {
  %v = load volatile i48, ptr addrspace(8) %p, align 1
  ret i48 %v
}

; Power-of-two widths keep their single helper call.
define i32 @load_i32(ptr addrspace(8) %p) {
  %v = load i32, ptr addrspace(8) %p, align 1
  ret i32 %v
}

define void @store_i16(ptr addrspace(8) %p, i16 %v) {
  store i16 %v, ptr addrspace(8) %p, align 1
  ret void
}

; CHECK-LABEL: load_i24:
; CHECK: lcall __mcs51_gptrget16
; CHECK: lcall __mcs51_gptrget8
; CHECK-LABEL: store_i24:
; CHECK: lcall __mcs51_gptrput16
; CHECK: lcall __mcs51_gptrput8
; CHECK-LABEL: load_i32:
; CHECK: lcall __mcs51_gptrget32
; CHECK-NOT: lcall __mcs51_gptrget
; CHECK-LABEL: store_i16:
; CHECK: lcall __mcs51_gptrput16
; CHECK-NOT: lcall __mcs51_gptrput

; LOWERED-LABEL: define i24 @load_i24
; LOWERED: call i16 @__mcs51_gptrget16(
; LOWERED: %gptr.byte.address{{[0-9]*}} = add i16 %gptr.byte.address{{[0-9]*}}, 2
; LOWERED: call i16 @__mcs51_gptrget8(
; LOWERED: shl i24 %{{.*}}, 16
; LOWERED-LABEL: define void @store_i24
; LOWERED: lshr i24 %v, 16
; LOWERED: add i16 %gptr.byte.address{{[0-9]*}}, 2
; LOWERED-LABEL: define i48 @load_i48
; LOWERED: call i32 @__mcs51_gptrget32(
; LOWERED: add i16 %gptr.byte.address{{[0-9]*}}, 4
; LOWERED: call i16 @__mcs51_gptrget16(
; LOWERED-LABEL: define i32 @load_i32
; LOWERED: call i32 @__mcs51_gptrget32(
; LOWERED-NOT: gptrget
; LOWERED: ret i32
