; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -O0 -verify-machineinstrs -filetype=null %s
; RUN: llc -mtriple=mcs51 -O2 -filetype=obj -o %t.o %s
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

declare i16 @llvm.bswap.i16(i16)
declare i32 @llvm.bswap.i32(i32)
declare i64 @llvm.bswap.i64(i64)

; The two bytes of a word trade places through A.
define i16 @swap16(i16 %x) {
  %r = call i16 @llvm.bswap.i16(i16 %x)
  ret i16 %r
}
; CHECK-LABEL: swap16:
; CHECK:      mov a, 48
; CHECK-NEXT: xch a, 49
; CHECK-NEXT: mov 48, a
; CHECK-NEXT: ret
; DIS-LABEL: <swap16>:
; DIS: e5 30 mov a, 48
; DIS: c5 31 xch a, 49
; DIS: f5 30 mov 48, a

define i32 @swap32(i32 %x) {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}
; CHECK-LABEL: swap32:
; CHECK-COUNT-2: xch a,
; CHECK: ret

define i64 @swap64(i64 %x) {
  %r = call i64 @llvm.bswap.i64(i64 %x)
  ret i64 %r
}
; CHECK-LABEL: swap64:
; CHECK: xch a,
; CHECK: ret

; A big-endian field read from XDATA: load, then exchange the bytes.
define i16 @load_swap16(ptr addrspace(4) %p) {
  %v = load i16, ptr addrspace(4) %p, align 1
  %r = call i16 @llvm.bswap.i16(i16 %v)
  ret i16 %r
}
; CHECK-LABEL: load_swap16:
; CHECK: movx a, @dptr
; CHECK: inc dptr
; CHECK: movx a, @dptr
; CHECK: xch a,
; CHECK: ret

define void @store_swap16(ptr addrspace(4) %p, i16 %v) {
  %r = call i16 @llvm.bswap.i16(i16 %v)
  store i16 %r, ptr addrspace(4) %p, align 1
  ret void
}
; CHECK-LABEL: store_swap16:
; CHECK: xch a,
; CHECK: movx @dptr, a
; CHECK: inc dptr
; CHECK: movx @dptr, a

; The address is a pointer plus an offset; volatile accesses keep their order.
define i16 @volatile_load_swap16(ptr addrspace(4) %p) {
  %q = getelementptr i8, ptr addrspace(4) %p, i16 22
  %v = load volatile i16, ptr addrspace(4) %q, align 1
  %r = call i16 @llvm.bswap.i16(i16 %v)
  ret i16 %r
}
; CHECK-LABEL: volatile_load_swap16:
; CHECK: add a, #22
; CHECK: movx a, @dptr
; CHECK: inc dptr
; CHECK: movx a, @dptr
; CHECK: xch a,

; Wide XDATA accesses are split into byte accesses at increasing offsets.
define i32 @load32(ptr addrspace(4) %p) {
  %v = load i32, ptr addrspace(4) %p, align 1
  ret i32 %v
}
; CHECK-LABEL: load32:
; CHECK-COUNT-3: inc dptr
; CHECK: ret

define i32 @load32_offset(ptr addrspace(4) %p) {
  %q = getelementptr i8, ptr addrspace(4) %p, i16 22
  %v = load i32, ptr addrspace(4) %q, align 1
  ret i32 %v
}
; CHECK-LABEL: load32_offset:
; CHECK: add a, #22
; CHECK-COUNT-3: inc dptr

define void @store32(ptr addrspace(4) %p, i32 %v) {
  store i32 %v, ptr addrspace(4) %p, align 1
  ret void
}
; CHECK-LABEL: store32:
; CHECK-COUNT-4: movx @dptr, a

define i32 @volatile_load32(ptr addrspace(4) %p) {
  %v = load volatile i32, ptr addrspace(4) %p, align 1
  ret i32 %v
}
; CHECK-LABEL: volatile_load32:
; CHECK-COUNT-4: movx a, @dptr

define void @volatile_store32(ptr addrspace(4) %p, i32 %v) {
  store volatile i32 %v, ptr addrspace(4) %p, align 1
  ret void
}
; CHECK-LABEL: volatile_store32:
; CHECK-COUNT-4: movx @dptr, a
