; RUN: not llc -mtriple=mcs51 -O0 %s -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -mtriple=mcs51 -O2 %s -o /dev/null 2>&1 | FileCheck %s
; CHECK: IDATA stack pointer escapes through an XDATA or incompatible address space

declare void @consume(ptr)
define void @escape(i16 %offset) {
  %local = alloca [8 x i8], addrspace(2)
  %bits = ptrtoint ptr addrspace(2) %local to i16
  %address = add i16 %bits, %offset
  %pointer = inttoptr i16 %address to ptr
  call void @consume(ptr %pointer)
  ret void
}
