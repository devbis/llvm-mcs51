; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CC2530
; RUN: llc -mtriple=mcs51 -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=CLASSIC

; A dynamic IDATA-to-default cast preserves null, and maps non-null IDATA
; addresses into CC2530's XDATA SRAM alias. Classic 8051 keeps its previous
; numeric conversion because it has no universal SRAM alias.
define ptr @convert(ptr addrspace(2) %p) {
; CC2530-LABEL: convert:
; CC2530: jz
; CC2530: mov {{r[0-7]}}, #31
; CC2530: mov {{r[0-7]}}, #0
; CLASSIC-LABEL: convert:
; CLASSIC-NOT: #31
; CLASSIC: mov {{r[0-7]}}, #0
; CLASSIC: ret
  %q = addrspacecast ptr addrspace(2) %p to ptr
  ret ptr %q
}
