; RUN: llc -mtriple=mcs51 -mcs51-overlay < %s | FileCheck %s
; RUN: llc -mtriple=mcs51 < %s | FileCheck --check-prefix=NO-OVERLAY %s

; The caller and callee are live at the same time and need distinct bytes.
define internal i8 @caller(i8 %value) {
entry:
  %slot = alloca i8, align 1
  store i8 %value, ptr %slot, align 1
  %call = call i8 @callee(i8 %value)
  %load = load i8, ptr %slot, align 1
  %result = add i8 %call, %load
  ret i8 %result
}

define internal i8 @callee(i8 %value) {
entry:
  %slot = alloca i8, align 1
  store i8 %value, ptr %slot, align 1
  %load = load i8, ptr %slot, align 1
  ret i8 %load
}

; An unrelated function can reuse either byte.
define internal i8 @sibling(i8 %value) {
entry:
  %slot = alloca i8, align 1
  store i8 %value, ptr %slot, align 1
  %load = load i8, ptr %slot, align 1
  ret i8 %load
}

; CHECK: .section .mcs51.data1.bss
; CHECK: __mcs51_overlay_data:
; CHECK: .zero 2

; NO-OVERLAY-NOT: __mcs51_overlay
