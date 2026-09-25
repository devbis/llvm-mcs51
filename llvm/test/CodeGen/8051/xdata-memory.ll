; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s

define i8 @read_xdata(i8 %address) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(4)
  %value = load i8, ptr addrspace(4) %pointer, align 1
  ret i8 %value
}

define void @write_xdata(i8 %address, i8 %value) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(4)
  store i8 %value, ptr addrspace(4) %pointer, align 1
  ret void
}

; CHECK-LABEL: read_xdata:
; CHECK: mov 131, #0
; CHECK: mov a, r7
; CHECK: mov 130, a
; CHECK: movx a, @dptr
; CHECK: mov r0, a
; CHECK: mov a, r0
; CHECK: ret

; CHECK-LABEL: write_xdata:
; CHECK: mov 131, #0
; CHECK: mov a, r7
; CHECK: mov 130, a
; CHECK: mov a, r6
; CHECK: movx @dptr, a
; CHECK: ret
