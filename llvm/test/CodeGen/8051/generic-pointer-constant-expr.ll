; RUN: llc -mtriple=mcs51 -O2 -verify-machineinstrs %s -o - | FileCheck %s

; Constant expressions that wrap a conversion to a generic pointer (here a
; constant GEP in the generic space) are expanded so the conversion is
; lowered; they used to reach instruction selection and abort with
; "Do not know how to expand the result of this operator".

; (u8 __generic *)(u8 __data *)0x30 + 3 == {0x33, 0x00, 0x40, 0x00}
; CHECK-LABEL: data_plus_3:
; CHECK:      mov 48, #51
; CHECK-NEXT: clr a
; CHECK-NEXT: mov 49, a
; CHECK-NEXT: mov 50, #64
; CHECK-NEXT: mov 51, a
; CHECK-NEXT: ret
define ptr addrspace(8) @data_plus_3() {
  ret ptr addrspace(8) getelementptr (i8, ptr addrspace(8) addrspacecast (ptr addrspace(1) inttoptr (i8 48 to ptr addrspace(1)) to ptr addrspace(8)), i32 3)
}

; (u8 __generic *)(u8 __pdata *)0x12 + 1 == {0x13, 0x00, 0x60, 0x00}
; CHECK-LABEL: pdata_plus_1:
; CHECK:      mov 48, #19
; CHECK:      mov 50, #96
define ptr addrspace(8) @pdata_plus_1() {
  ret ptr addrspace(8) getelementptr (i8, ptr addrspace(8) addrspacecast (ptr addrspace(3) inttoptr (i8 18 to ptr addrspace(3)) to ptr addrspace(8)), i32 1)
}

; A generic pointer into a DATA object passed directly as a call argument.
; CHECK-LABEL: pass_object_plus_2:
; CHECK:      mov r0, #d
; CHECK-NEXT: mov a, r0
; CHECK-NEXT: add a, #2
; CHECK-NEXT: mov 48, a
; CHECK:      mov 50, #64
; CHECK:      lcall take
@d = addrspace(1) global [4 x i8] zeroinitializer
declare void @take(ptr addrspace(8))
define void @pass_object_plus_2() {
  call void @take(ptr addrspace(8) getelementptr (i8, ptr addrspace(8) addrspacecast (ptr addrspace(1) @d to ptr addrspace(8)), i32 2))
  ret void
}
