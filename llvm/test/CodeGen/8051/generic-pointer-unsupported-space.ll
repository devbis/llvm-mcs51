; BIT and SFR objects have no indirectly addressable location, so they cannot
; be converted to a generic pointer.
; RUN: split-file %s %t
; RUN: not llc -mtriple=mcs51 %t/sfr.ll -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -mtriple=mcs51 %t/bit.ll -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -mtriple=mcs51 %t/const.ll -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -mtriple=mcs51 %t/init.ll -o /dev/null 2>&1 \
; RUN:   | FileCheck %s --check-prefix=INIT

; INIT: error: conversion of a BIT or SFR address to a generic pointer in a static initializer
; INIT-NOT: PLEASE submit a bug report

; CHECK: error: {{.*}}: in function {{.*}}: conversion of a BIT or SFR address to a generic pointer
; CHECK-NOT: PLEASE submit a bug report

;--- sfr.ll
define ptr addrspace(8) @sfr_to_generic(ptr addrspace(7) %p) {
  %g = addrspacecast ptr addrspace(7) %p to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

;--- bit.ll
define ptr addrspace(8) @bit_to_generic(ptr addrspace(6) %p) {
  %g = addrspacecast ptr addrspace(6) %p to ptr addrspace(8)
  ret ptr addrspace(8) %g
}

;--- const.ll
@flag = external addrspace(6) global i1

define ptr addrspace(8) @bit_object_to_generic() {
  ret ptr addrspace(8) addrspacecast (ptr addrspace(6) @flag to ptr addrspace(8))
}

;--- init.ll
@reg = external addrspace(7) global i8
@table = addrspace(4) global ptr addrspace(8) addrspacecast (ptr addrspace(7) @reg to ptr addrspace(8)), align 1
