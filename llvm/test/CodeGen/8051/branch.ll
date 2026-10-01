; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s

define i8 @jump_to_return() {
entry:
  br label %done

done:
  ret i8 7
}

; CHECK-LABEL: jump_to_return:
; CHECK: mov a, #7
; CHECK: ret
