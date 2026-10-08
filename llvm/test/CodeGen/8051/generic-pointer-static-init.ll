; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=mcs51 -O2 -filetype=obj %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC

; A generic pointer initializer is {address low, address high, tag, 0}. The
; converted address keeps its own 1- or 2-byte relocation and the tag is a
; constant byte, because there is no 32-bit data relocation. The global keeps
; its size and every field keeps its offset.

; Code still addresses the rewritten globals with their original layout.
; CHECK-LABEL: load_second:
; CHECK: mov dptr, #xdata16(gt+4)
define ptr addrspace(8) @load_second() {
  %e = getelementptr [7 x ptr addrspace(8)], ptr addrspace(4) @gt, i16 0, i16 1
  %v = load ptr addrspace(8), ptr addrspace(4) %e
  ret ptr addrspace(8) %v
}

@d = addrspace(1) global [4 x i8] zeroinitializer
@i = addrspace(2) global [4 x i8] zeroinitializer
@p = addrspace(3) global [4 x i8] zeroinitializer
@x = addrspace(4) global [4 x i8] zeroinitializer
@c = addrspace(5) constant [4 x i8] c"\01\02\03\04"

; CHECK-LABEL: gt:
; CHECK-NEXT:  .byte d
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 64
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte i+1
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 64
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte p+2
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 96
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .short x+3
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .short c
; CHECK-NEXT:  .byte 128
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 51
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 64
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .long 0
; CHECK-NEXT:  .size gt, 28
@gt = addrspace(4) global [7 x ptr addrspace(8)] [
  ptr addrspace(8) addrspacecast (ptr addrspace(1) @d to ptr addrspace(8)),
  ptr addrspace(8) addrspacecast (ptr addrspace(2) getelementptr (i8, ptr addrspace(2) @i, i8 1) to ptr addrspace(8)),
  ptr addrspace(8) getelementptr (i8, ptr addrspace(8) addrspacecast (ptr addrspace(3) @p to ptr addrspace(8)), i32 2),
  ptr addrspace(8) addrspacecast (ptr addrspace(4) getelementptr (i8, ptr addrspace(4) @x, i16 3) to ptr addrspace(8)),
  ptr addrspace(8) addrspacecast (ptr addrspace(5) @c to ptr addrspace(8)),
  ptr addrspace(8) addrspacecast (ptr addrspace(1) inttoptr (i8 51 to ptr addrspace(1)) to ptr addrspace(8)),
  ptr addrspace(8) null], align 1

; CHECK-LABEL: s:
; CHECK-NEXT:  .byte 7
; CHECK-NEXT:  .byte d+2
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 64
; CHECK-NEXT:  .byte 0
; CHECK-NEXT:  .byte 9
; CHECK-NEXT:  .size s, 6
@s = addrspace(4) global { i8, ptr addrspace(8), i8 } { i8 7, ptr addrspace(8) addrspacecast (ptr addrspace(1) getelementptr (i8, ptr addrspace(1) @d, i8 2) to ptr addrspace(8)), i8 9 }, align 1

; RELOC:      Section ({{.*}}) .rela.mcs51.xdata.init {
; RELOC-NEXT:   0x0 R_8051_8 d 0x0
; RELOC-NEXT:   0x4 R_8051_8 i 0x1
; RELOC-NEXT:   0x8 R_8051_8 p 0x2
; RELOC-NEXT:   0xC R_8051_16 x 0x3
; RELOC-NEXT:   0x10 R_8051_16 c 0x0
; RELOC-NEXT:   0x1D R_8051_8 d 0x2
; RELOC-NEXT: }
