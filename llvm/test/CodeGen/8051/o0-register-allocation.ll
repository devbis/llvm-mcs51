; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -O0 < %s -o /dev/null
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -O0 -debug-pass=Structure < %s 2>&1 | FileCheck %s --check-prefix=O0-RA

; -O0 uses Greedy because Fast can create spill frames larger than the 8051's
; addressable stack frame.
; O0-RA: Greedy Register Allocator
; O0-RA-NOT: Fast Register Allocator

define i16 @o0_regalloc_smoke(i16 %a, i16 %b) {
entry:
  %sum = add i16 %a, %b
  ret i16 %sum
}
