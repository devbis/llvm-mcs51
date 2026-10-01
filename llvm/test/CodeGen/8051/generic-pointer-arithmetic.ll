; RUN: llc -mtriple=mcs51 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -print-after=inline-asm-prepare -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=LOWERED

define ptr addrspace(8) @advance_generic_byte(ptr addrspace(8) %pointer,
                                               i16 %count) {
entry:
  %next = getelementptr i8, ptr addrspace(8) %pointer, i16 %count
  ret ptr addrspace(8) %next
}

define ptr addrspace(8) @advance_generic_word(ptr addrspace(8) %pointer,
                                               i16 %count) {
entry:
  %next = getelementptr i16, ptr addrspace(8) %pointer, i16 %count
  ret ptr addrspace(8) %next
}

; CHECK-LABEL: advance_generic_byte:
; CHECK: mov
; CHECK-LABEL: advance_generic_word:
; CHECK: mov

; LOWERED-LABEL: define ptr addrspace(8) @advance_generic_byte
; LOWERED: %gptr.gep.address = trunc i32 %gptr.gep.base to i16
; LOWERED: %gptr.gep.tag = and i32 %gptr.gep.base, -65536
; LOWERED: %gptr.gep.bits = or{{.*}}i32 %gptr.gep.tag
; LOWERED-LABEL: define ptr addrspace(8) @advance_generic_word
; LOWERED: %gptr.gep.scaled.index = shl i16 %count, 1
; LOWERED: %gptr.gep.tag = and i32 %gptr.gep.base, -65536
