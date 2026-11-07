; RUN: not --crash llc -mtriple=mcs51 -O2 -o /dev/null %s 2>&1 | FileCheck %s

; A bit has no register-indirect form: SETB/CLR/MOV C encode the bit address in
; the instruction, so a bit index known only at run time cannot be selected.
; Diagnose it instead of failing with "Cannot select".

@bits = external addrspace(6) global [8 x i8]

; CHECK: LLVM ERROR: MCS-51 bit accesses need a constant or bit-addressed global address; a variable bit index is not supported
define void @store_indexed(i8 %i) {
  %p = getelementptr i8, ptr addrspace(6) @bits, i8 %i
  store volatile i8 1, ptr addrspace(6) %p, align 1
  ret void
}
