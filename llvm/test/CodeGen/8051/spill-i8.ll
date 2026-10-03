; RUN: llc -O0 -mtriple=mcs51 -mcpu=cc2530 -o - %s | FileCheck %s
; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s --check-prefix=GENERIC

define i8 @sum_ten(i8 %a, i8 %b, i8 %c, i8 %d, i8 %e, i8 %f, i8 %g,
                   i8 %h, i8 %i, i8 %j) {
entry:
  %ab = add i8 %a, %b
  %abc = add i8 %ab, %c
  %abcd = add i8 %abc, %d
  %abcde = add i8 %abcd, %e
  %abcdef = add i8 %abcde, %f
  %abcdefg = add i8 %abcdef, %g
  %abcdefgh = add i8 %abcdefg, %h
  %abcdefghi = add i8 %abcdefgh, %i
  %abcdefghij = add i8 %abcdefghi, %j
  ret i8 %abcdefghij
}

; CHECK-LABEL: sum_ten:
; CHECK: mov a, 129
; CHECK-NEXT: add a, #4
; CHECK-NEXT: mov 129, a
; CHECK: mov @r1, {{r[0-7]|a}}
; CHECK: dec 129
; CHECK: ret

declare void @entry_barrier()

define i8 @main(i8 %a, i8 %b, i8 %c, i8 %d, i8 %e, i8 %f, i8 %g, i8 %h,
                i8 %i, i8 %j) {
entry:
  call void @entry_barrier()
  %ab = add i8 %a, %b
  %abc = add i8 %ab, %c
  %abcd = add i8 %abc, %d
  %abcde = add i8 %abcd, %e
  %abcdef = add i8 %abcde, %f
  %abcdefg = add i8 %abcdef, %g
  %abcdefgh = add i8 %abcdefg, %h
  %abcdefghi = add i8 %abcdefgh, %i
  %abcdefghij = add i8 %abcdefghi, %j
  ret i8 %abcdefghij
}

; The freestanding entry point is not called by ordinary C code and need not
; restore the register bank when it returns to the startup halt loop.
; CHECK-LABEL: main:
; CHECK-NOT: push
; CHECK: lcall __mcs51_bankcall_entry_barrier
; CHECK-NOT: pop
; CHECK-NOT: mov 129, a
; CHECK: ret

; Other MCS-51 profiles retain the ordinary function ABI for main.
; GENERIC-LABEL: main:
; GENERIC: lcall __mcs51_save_r{{[0-7]}}
; GENERIC: lcall entry_barrier
; GENERIC: lcall __mcs51_restore_r{{[0-7]}}
