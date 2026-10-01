; RUN: llc -mtriple=mcs51 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -filetype=obj -o %t.o %s
; RUN: ld.lld -e call_void -T %S/../../../lib/Target/MCS51/cc2530.ld --no-check-sections -o %t.elf %t.o
; RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

target triple = "mcs51"

define void @call_void(ptr %fn) {
entry:
  call void %fn()
  ret void
}

; CHECK-LABEL: call_void:
; CHECK: lcall .Lcall_void.mcs51.icall
; CHECK: ret
; CHECK-LABEL: .Lcall_void.mcs51.icall:
; CHECK: clr a
; CHECK: jmp @a+dptr

; DIS-LABEL: <call_void>:
; DIS: lcall
; DIS: ret
; DIS: clr a
; DIS: jmp @a+dptr
