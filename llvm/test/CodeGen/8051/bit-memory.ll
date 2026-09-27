; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -filetype=obj %s -o %t.o
; RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

@bit_flag = external addrspace(6) global i8

define i8 @read_bit_flag() {
entry:
  %value = load i8, ptr addrspace(6) @bit_flag, align 1
  ret i8 %value
}

define void @write_bit_flag(i8 %value) {
entry:
  store i8 %value, ptr addrspace(6) @bit_flag, align 1
  ret void
}

; CHECK-LABEL: read_bit_flag:
; CHECK: mov c, bit_flag
; CHECK-NEXT: clr a
; CHECK-NEXT: rlc a
; CHECK-NEXT: ret
; CHECK-LABEL: write_bit_flag:
; CHECK: mov c, 224
; CHECK-NEXT: mov bit_flag, c

; RELOC: R_8051_8 bit_flag
; RELOC: R_8051_8 bit_flag
