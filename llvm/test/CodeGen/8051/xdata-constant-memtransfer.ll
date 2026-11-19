; RUN: llc -mtriple=mcs51 -verify-machineinstrs %s -o - | FileCheck %s

declare void @llvm.memcpy.p4.p4.i16(ptr addrspace(4), ptr addrspace(4), i16, i1 immarg)
declare void @llvm.memcpy.inline.p4.p4.i16(ptr addrspace(4), ptr addrspace(4), i16 immarg, i1 immarg)
declare void @llvm.memmove.p4.p4.i16(ptr addrspace(4), ptr addrspace(4), i16, i1 immarg)
declare void @llvm.memmove.p4.p4.i32(ptr addrspace(4), ptr addrspace(4), i32, i1 immarg)

define void @copy_large(ptr addrspace(4) %d, ptr addrspace(4) %s) {
; CHECK-LABEL: copy_large:
; CHECK: lcall memcpy
  call void @llvm.memcpy.p4.p4.i16(ptr addrspace(4) %d, ptr addrspace(4) %s, i16 40, i1 false)
  ret void
}

define void @move_large(ptr addrspace(4) %d, ptr addrspace(4) %s) {
; CHECK-LABEL: move_large:
; CHECK: lcall memmove
  call void @llvm.memmove.p4.p4.i16(ptr addrspace(4) %d, ptr addrspace(4) %s, i16 40, i1 false)
  ret void
}

define void @copy_inline(ptr addrspace(4) %d, ptr addrspace(4) %s) {
; CHECK-LABEL: copy_inline:
; CHECK-NOT: lcall memcpy
; CHECK: movx
; CHECK-NOT: lcall memcpy
; CHECK: ret
  call void @llvm.memcpy.inline.p4.p4.i16(ptr addrspace(4) %d, ptr addrspace(4) %s, i16 40, i1 false)
  ret void
}

define void @copy_volatile(ptr addrspace(4) %d, ptr addrspace(4) %s) {
; CHECK-LABEL: copy_volatile:
; CHECK-NOT: lcall memcpy
; CHECK: movx
; CHECK-NOT: lcall memcpy
; CHECK: ret
  call void @llvm.memcpy.p4.p4.i16(ptr addrspace(4) %d, ptr addrspace(4) %s, i16 40, i1 true)
  ret void
}

define void @move_volatile_i32(ptr addrspace(4) %d, ptr addrspace(4) %s) {
; CHECK-LABEL: move_volatile_i32:
; CHECK-NOT: lcall memmove
; CHECK: movx
; CHECK-NOT: lcall memmove
; CHECK: ret
  call void @llvm.memmove.p4.p4.i32(ptr addrspace(4) %d, ptr addrspace(4) %s, i32 40, i1 true)
  ret void
}

define void @copy_selected(ptr addrspace(4) %d, ptr addrspace(4) %a, ptr addrspace(4) %b, i1 %which) {
; CHECK-LABEL: copy_selected:
; CHECK: lcall memcpy
  %src = select i1 %which, ptr addrspace(4) %a, ptr addrspace(4) %b
  call void @llvm.memcpy.p4.p4.i16(ptr addrspace(4) %d, ptr addrspace(4) %src, i16 40, i1 false)
  ret void
}

declare void @llvm.memcpy.p4.p0.i16(ptr addrspace(4), ptr, i16, i1 immarg)
declare void @fill(ptr addrspace(2))

define void @copy_from_stack(ptr addrspace(4) %d) {
; CHECK-LABEL: copy_from_stack:
; CHECK: lcall fill
; CHECK-NOT: lcall memcpy
; CHECK: mov a, @r
; CHECK: movx @dptr, a
; CHECK-NOT: lcall memcpy
; CHECK: ret
  %local = alloca [8 x i8], addrspace(2)
  call void @fill(ptr addrspace(2) %local)
  %source = addrspacecast ptr addrspace(2) %local to ptr
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %d, ptr %source, i16 8, i1 false)
  ret void
}
