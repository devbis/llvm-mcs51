; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs %s -o - | FileCheck %s

; Generic pointers are 4 bytes: address low, address high, memory-space tag
; (XDATA 0x00, DATA/IDATA 0x40, PDATA 0x60, CODE 0x80) and a zero pad byte.
; Every address space's null pointer converts to the all-zero generic null.
; Generic pointer returns and arguments use the bytes at 48..51.

@d = addrspace(1) global i8 1
@i = addrspace(2) global i8 2
@p = addrspace(3) global i8 3
@x = addrspace(4) global i8 4
@c = addrspace(5) constant i8 5
@tab = addrspace(4) global [3 x ptr addrspace(8)] zeroinitializer

declare void @take_d(ptr addrspace(1))
declare void @take_g(ptr addrspace(8))

define void @pass_data_object() {
  call void @take_d(ptr addrspace(1) @d)
  ret void
}

define ptr addrspace(8) @data_object_to_generic() {
  ret ptr addrspace(8) addrspacecast (ptr addrspace(1) @d to ptr addrspace(8))
}

define ptr addrspace(8) @data_to_generic(ptr addrspace(1) %q) {
  %g = addrspacecast ptr addrspace(1) %q to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

define ptr addrspace(8) @idata_to_generic(ptr addrspace(2) %q) {
  %g = addrspacecast ptr addrspace(2) %q to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

define ptr addrspace(8) @pdata_to_generic(ptr addrspace(3) %q) {
  %g = addrspacecast ptr addrspace(3) %q to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

define ptr addrspace(8) @xdata_to_generic(ptr addrspace(4) %q) {
  %g = addrspacecast ptr addrspace(4) %q to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

define ptr addrspace(8) @code_to_generic(ptr addrspace(5) %q) {
  %g = addrspacecast ptr addrspace(5) %q to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

define ptr addrspace(1) @generic_to_data(ptr addrspace(8) %g) {
  %q = addrspacecast ptr addrspace(8) %g to ptr addrspace(1)
  ret ptr addrspace(1) %q
}

define void @store_generic(ptr addrspace(8) %g) {
  %e = getelementptr [3 x ptr addrspace(8)], ptr addrspace(4) @tab, i16 0, i16 2
  store ptr addrspace(8) %g, ptr addrspace(4) %e
  ret void
}

define ptr addrspace(8) @load_generic() {
  %e = getelementptr ptr addrspace(8), ptr addrspace(4) @tab, i16 1
  %g = load ptr addrspace(8), ptr addrspace(4) %e
  ret ptr addrspace(8) %g
}

define void @pass_generic_objects() {
  call void @take_g(ptr addrspace(8) addrspacecast (ptr addrspace(2) @i to ptr addrspace(8)))
  call void @take_g(ptr addrspace(8) addrspacecast (ptr addrspace(5) @c to ptr addrspace(8)))
  ret void
}

; A DATA object's address is one byte.
; CHECK-LABEL: pass_data_object:
; CHECK: mov r7, #d
; CHECK-NEXT: lcall take_d

; CHECK-LABEL: data_object_to_generic:
; CHECK: mov r0, #d
; CHECK-NEXT: mov 48, r0
; CHECK-NEXT: clr a
; CHECK-NEXT: mov 49, a
; CHECK-NEXT: mov 50, #64
; CHECK-NEXT: mov 51, a
; CHECK-NEXT: ret

; A run-time DATA pointer keeps a zero tag when it is null.
; CHECK-LABEL: data_to_generic:
; CHECK: cjne r7, #0,
; CHECK: mov 50, #0
; CHECK: mov 50, #64
; CHECK: mov 51, a
; CHECK-NEXT: mov 48, r7
; CHECK-NEXT: mov 49, a
; CHECK-NEXT: ret

; CHECK-LABEL: idata_to_generic:
; CHECK: cjne r7, #0,
; CHECK: mov 50, #0
; CHECK: mov 50, #64
; CHECK: mov 48, r7

; CHECK-LABEL: pdata_to_generic:
; CHECK: cjne r7, #0,
; CHECK: mov 50, #0
; CHECK: mov 50, #96
; CHECK: mov 48, r7

; XDATA has tag 0, so its generic form needs no null check.
; CHECK-LABEL: xdata_to_generic:
; CHECK: clr a
; CHECK-NEXT: mov 50, a
; CHECK-NEXT: mov 51, a
; CHECK-NEXT: ret

; CHECK-LABEL: code_to_generic:
; CHECK: mov a, 48
; CHECK-NEXT: orl a, 49
; CHECK-NEXT: jnz [[NONNULL:.LBB[0-9_]+]]
; CHECK: clr a
; CHECK-NEXT: mov 50, a
; CHECK-NEXT: mov 51, a
; CHECK-NEXT: ret
; CHECK: [[NONNULL]]:
; CHECK-NEXT: mov 50, #128
; CHECK-NEXT: mov 51, #0

; Conversion back to a one-byte space keeps the low address byte.
; CHECK-LABEL: generic_to_data:
; CHECK: mov r0, 48
; CHECK-NEXT: mov a, r0
; CHECK-NEXT: ret

; Generic pointer elements are 4 bytes apart and stored/loaded whole.
; CHECK-LABEL: store_generic:
; CHECK: mov dptr, #tab+10
; CHECK: mov a, 51
; CHECK: mov dptr, #tab+8
; CHECK: mov a, 48
; CHECK: mov a, 49
; CHECK-LABEL: load_generic:
; CHECK: mov dptr, #tab+4
; CHECK: mov 48, a
; CHECK: mov 49, a
; CHECK: mov 50, a
; CHECK: mov 51, a
; CHECK-NEXT: ret

; CHECK-LABEL: pass_generic_objects:
; CHECK: mov r0, #i
; CHECK: mov 50, #64
; CHECK: lcall take_g
; CHECK: mov 48, #lo8(c)
; CHECK-NEXT: mov 49, #hi8(c)
; CHECK-NEXT: mov 50, #128
; CHECK-NEXT: mov 51, #0
; CHECK-NEXT: lcall take_g

; CHECK: .bss
; CHECK: tab:
; CHECK-NEXT: .zero 12
