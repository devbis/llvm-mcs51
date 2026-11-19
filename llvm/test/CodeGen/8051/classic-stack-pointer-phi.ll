; RUN: not llc -mtriple=mcs51 -O0 %s -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -mtriple=mcs51 -O2 %s -o /dev/null 2>&1 | FileCheck %s
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -verify-machineinstrs %s -o /dev/null
; CHECK: IDATA stack pointer escapes through an XDATA or incompatible address space

declare void @consume(ptr)
define void @escape_phi(i1 %choose, ptr %external) {
entry:
  %local = alloca i8, addrspace(2)
  %stack = addrspacecast ptr addrspace(2) %local to ptr
  br i1 %choose, label %local.path, label %external.path
local.path:
  br label %join
external.path:
  br label %join
join:
  %pointer = phi ptr [ %stack, %local.path ], [ %external, %external.path ]
  call void @consume(ptr %pointer)
  ret void
}
