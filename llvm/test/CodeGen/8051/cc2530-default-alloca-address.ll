; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -verify-machineinstrs %s -o - | FileCheck %s

; LLVM IR can explicitly allocate in AS0 even though Clang uses AS2.
define ptr @default_alloca() {
; CHECK-LABEL: default_alloca:
; CHECK: mov {{r[0-7]}}, #31
; CHECK: ret
  %v = alloca i8
  ret ptr %v
}


; Frame indices are non-null and can use the alias without a runtime test.
define ptr @local_ptr() {
; CHECK-LABEL: local_ptr:
; CC2530: mov {{r[0-7]}}, #31
; CHECK-NOT: jz
; CC2530: ret
  %v = alloca i8, addrspace(2)
  %q = addrspacecast ptr addrspace(2) %v to ptr
  ret ptr %q
}
