; Conversions from CODE to a data pointer must name a CODE object whose
; initializer is visible, so that an XDATA copy can be made.
; RUN: split-file %s %t
; RUN: not llc -mtriple=mcs51 -mcpu=cc2530 -o /dev/null %t/extern.ll 2>&1 | FileCheck %s --check-prefix=EXTERN
; RUN: not llc -mtriple=mcs51 -mcpu=cc2530 -o /dev/null %t/unknown.ll 2>&1 | FileCheck %s --check-prefix=UNKNOWN
; RUN: not llc -mtriple=mcs51 -mcpu=cc2530 -o /dev/null %t/idata.ll 2>&1 | FileCheck %s --check-prefix=IDATA

; EXTERN: error: {{.*}}MCS-51 CODE object 'external_table' is used as an XDATA pointer but its initializer is not available in this translation unit
; UNKNOWN: error: {{.*}}MCS-51 cannot identify the CODE object of an address used as an XDATA pointer
; IDATA: error: {{.*}}MCS-51 cannot convert a CODE address to a DATA, IDATA, PDATA, BIT or SFR pointer

;--- extern.ll
@external_table = external addrspace(5) constant [4 x i8]
declare void @sink(ptr)
define void @escape_external() {
  call void @sink(ptr addrspacecast (ptr addrspace(5) @external_table to ptr))
  ret void
}

;--- unknown.ll
declare void @sink(ptr)
define void @escape_argument(ptr addrspace(5) %p) {
  %d = addrspacecast ptr addrspace(5) %p to ptr
  call void @sink(ptr %d)
  ret void
}

;--- idata.ll
@table = internal addrspace(5) constant [2 x i8] c"\01\02"
declare void @sink_idata(ptr addrspace(2))
define void @escape_idata() {
  %d = addrspacecast ptr addrspace(5) @table to ptr addrspace(2)
  call void @sink_idata(ptr addrspace(2) %d)
  ret void
}
