; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s
; RUN: llc -O0 -mtriple=mcs51 -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

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

define i8 @read_idata(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(2)
  %value = load i8, ptr addrspace(2) %pointer, align 1
  ret i8 %value
}

define void @write_idata(i8 %address, i8 %value) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(2)
  store i8 %value, ptr addrspace(2) %pointer, align 1
  ret void
}

define i8 @read_pdata(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(3)
  %value = load i8, ptr addrspace(3) %pointer, align 1
  ret i8 %value
}

define void @write_pdata(i8 %address, i8 %value) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(3)
  store i8 %value, ptr addrspace(3) %pointer, align 1
  ret void
}

define i8 @read_code(i8 %address) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(5)
  %value = load i8, ptr addrspace(5) %pointer, align 1
  ret i8 %value
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

; CHECK-LABEL: read_idata:
; CHECK: mov a, @r{{[01]}}
; CHECK: ret

; CHECK-LABEL: write_idata:
; CHECK: mov a, r6
; CHECK: mov @r{{[01]}}, a
; CHECK: ret

; CHECK-LABEL: read_pdata:
; CHECK: movx a, @r{{[01]}}
; CHECK: ret

; CHECK-LABEL: write_pdata:
; CHECK: mov a, r6
; CHECK: movx @r{{[01]}}, a
; CHECK: ret

; CHECK-LABEL: read_code:
; CHECK: mov 131, #0
; CHECK: mov a, r7
; CHECK: mov 130, a
; CHECK: clr a
; CHECK: movc a, @a+dptr
; CHECK: ret

; DIS-LABEL: <read_idata>:
; DIS: mov a, @r0
; DIS-LABEL: <write_idata>:
; DIS: mov @r0, a
; DIS-LABEL: <read_pdata>:
; DIS: movx a, @r0
; DIS-LABEL: <write_pdata>:
; DIS: movx @r0, a
; DIS-LABEL: <read_code>:
; DIS: movc a, @a+dptr
