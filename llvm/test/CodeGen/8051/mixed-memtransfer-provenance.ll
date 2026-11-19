; RUN: llc -mtriple=mcs51 -verify-machineinstrs %s -o - | FileCheck %s

declare void @llvm.memcpy.p4.p0.i16(ptr addrspace(4), ptr, i16, i1 immarg)
declare void @llvm.memcpy.p0.p4.i16(ptr, ptr addrspace(4), i16, i1 immarg)
declare void @llvm.memmove.p0.p0.i16(ptr, ptr, i16, i1 immarg)

define void @copy_selected_source(ptr addrspace(4) %dst, ptr %external, i1 %choose, i16 %count) {
; CHECK-LABEL: copy_selected_source:
; CHECK: #64
; CHECK: lcall __mcs51_gptrget8
; CHECK: movx @dptr, a
  %local = alloca [8 x i8]
  %selected = select i1 %choose, ptr %local, ptr %external
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %dst, ptr %selected, i16 %count, i1 false)
  ret void
}

define void @copy_selected_destination(ptr addrspace(4) %src, ptr %external, i1 %choose) {
; CHECK-LABEL: copy_selected_destination:
; CHECK: #64
; CHECK: movx a, @dptr
; CHECK: lcall __mcs51_gptrput8
  %local = alloca [8 x i8]
  %selected = select i1 %choose, ptr %local, ptr %external
  call void @llvm.memcpy.p0.p4.i16(ptr %selected, ptr addrspace(4) %src, i16 4, i1 true)
  ret void
}

define void @copy_phi_source(ptr addrspace(4) %dst, ptr %external, i1 %choose) {
; CHECK-LABEL: copy_phi_source:
; CHECK: #64
; CHECK: lcall __mcs51_gptrget8
  %local = alloca [8 x i8]
  br i1 %choose, label %stack, label %other
stack:
  br label %join
other:
  br label %join
join:
  %selected = phi ptr [ %local, %stack ], [ %external, %other ]
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %dst, ptr %selected, i16 4, i1 false)
  ret void
}

define void @move_gep_selected(ptr %external, i1 %choose, i16 %count) {
; CHECK-LABEL: move_gep_selected:
; CHECK: #64
; CHECK: lcall __mcs51_gptrget8
; CHECK: lcall __mcs51_gptrput8
  %local = alloca [16 x i8]
  %selected = select i1 %choose, ptr %local, ptr %external
  %next = getelementptr i8, ptr %selected, i16 1
  call void @llvm.memmove.p0.p0.i16(ptr %next, ptr %selected, i16 %count, i1 false)
  ret void
}

define void @copy_selected_idata(ptr addrspace(4) %dst, ptr addrspace(2) %inner, ptr addrspace(4) %outer, i1 %choose, i16 %count) {
; CHECK-LABEL: copy_selected_idata:
; CHECK: #64
; CHECK: lcall __mcs51_gptrget8
; CHECK: movx @dptr, a
  %ip = addrspacecast ptr addrspace(2) %inner to ptr
  %xp = addrspacecast ptr addrspace(4) %outer to ptr
  %selected = select i1 %choose, ptr %ip, ptr %xp
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %dst, ptr %selected, i16 %count, i1 false)
  ret void
}

define void @copy_default_xdata(ptr addrspace(4) %dst, ptr %src) {
; CHECK-LABEL: copy_default_xdata:
; CHECK: lcall memcpy
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %dst, ptr %src, i16 40, i1 false)
  ret void
}

define void @move_default_xdata(ptr addrspace(4) %dst, ptr %src) {
; CHECK-LABEL: move_default_xdata:
; CHECK: lcall memmove
  %d = addrspacecast ptr addrspace(4) %dst to ptr
  call void @llvm.memmove.p0.p0.i16(ptr %d, ptr %src, i16 40, i1 false)
  ret void
}

define void @copy_selected_pdata(ptr addrspace(4) %dst, ptr addrspace(3) %page, ptr addrspace(4) %ram, i1 %choose) {
; CHECK-LABEL: copy_selected_pdata:
; CHECK: #96
; CHECK: lcall __mcs51_gptrget8
  %pp = addrspacecast ptr addrspace(3) %page to ptr
  %xp = addrspacecast ptr addrspace(4) %ram to ptr
  %selected = select i1 %choose, ptr %pp, ptr %xp
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %dst, ptr %selected, i16 4, i1 false)
  ret void
}
