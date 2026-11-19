; RUN: not llc -mtriple=mcs51 %s -o /dev/null 2>&1 | FileCheck %s

declare void @llvm.memcpy.p4.p0.i16(ptr addrspace(4), ptr, i16, i1 immarg)

define void @copy_bit(ptr addrspace(4) %dst, ptr addrspace(6) %bit, ptr %other, i1 %choose, i16 %count) {
; CHECK: conversion of a BIT or SFR address to a generic pointer
  %p = addrspacecast ptr addrspace(6) %bit to ptr
  %selected = select i1 %choose, ptr %p, ptr %other
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) %dst, ptr %selected, i16 %count, i1 false)
  ret void
}
