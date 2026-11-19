; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -verify-machineinstrs %s -o - | FileCheck %s

; LLVM IR can explicitly allocate in AS0 even though Clang uses AS2.
define ptr @default_alloca() {
; CHECK-LABEL: default_alloca:
; CHECK: mov {{r[0-7]}}, #31
; CHECK: ret
  %v = alloca i8
  ret ptr %v
}
