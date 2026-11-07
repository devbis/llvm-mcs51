; RUN: llc -mtriple=mcs51 -O2 -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -O0 -o - %s | FileCheck %s

; The optimizer can merge `if (c) A = v; else B = v;` into one access through a
; select of two bit addresses. SETB/CLR/MOV C take the bit address as an
; immediate, so the access must be split back into one access per address,
; whatever the stored value is.

@P43 = external addrspace(6) global i8
@P44 = external addrspace(6) global i8
@P45 = external addrspace(6) global i8

; CHECK-LABEL: store_one:
; CHECK-DAG: setb P43
; CHECK-DAG: setb P44
define void @store_one(i1 %c) {
  %p = select i1 %c, ptr addrspace(6) @P43, ptr addrspace(6) @P44
  store volatile i8 1, ptr addrspace(6) %p, align 1
  ret void
}

; CHECK-LABEL: store_zero:
; CHECK-DAG: clr P43
; CHECK-DAG: clr P44
define void @store_zero(i1 %c) {
  %p = select i1 %c, ptr addrspace(6) @P43, ptr addrspace(6) @P44
  store volatile i8 0, ptr addrspace(6) %p, align 1
  ret void
}

; CHECK-LABEL: store_value:
; CHECK-DAG: mov P43, c
; CHECK-DAG: mov P44, c
define void @store_value(i1 %c, i8 %v) {
  %p = select i1 %c, ptr addrspace(6) @P43, ptr addrspace(6) @P44
  store volatile i8 %v, ptr addrspace(6) %p, align 1
  ret void
}

; CHECK-LABEL: load_value:
; CHECK-DAG: mov c, P43
; CHECK-DAG: mov c, P44
define i8 @load_value(i1 %c) {
  %p = select i1 %c, ptr addrspace(6) @P43, ptr addrspace(6) @P44
  %v = load volatile i8, ptr addrspace(6) %p, align 1
  ret i8 %v
}

; A select of selects splits into three accesses.
; CHECK-LABEL: nested_select:
; CHECK-DAG: setb P43
; CHECK-DAG: setb P44
; CHECK-DAG: setb P45
define void @nested_select(i1 %c, i1 %d) {
  %q = select i1 %d, ptr addrspace(6) @P44, ptr addrspace(6) @P45
  %p = select i1 %c, ptr addrspace(6) @P43, ptr addrspace(6) %q
  store volatile i8 1, ptr addrspace(6) %p, align 1
  ret void
}

; The direct-addressed DATA and SFR spaces need an immediate address as well.
@D1 = external addrspace(1) global i8
@D2 = external addrspace(1) global i8
@S1 = external addrspace(7) global i8
@S2 = external addrspace(7) global i8

; CHECK-LABEL: data_select:
; CHECK-DAG: mov D1,
; CHECK-DAG: mov D2,
define void @data_select(i1 %c) {
  %p = select i1 %c, ptr addrspace(1) @D1, ptr addrspace(1) @D2
  store volatile i8 1, ptr addrspace(1) %p, align 1
  ret void
}

; CHECK-LABEL: sfr_select:
; CHECK-DAG: mov S1,
; CHECK-DAG: mov S2,
define void @sfr_select(i1 %c) {
  %p = select i1 %c, ptr addrspace(7) @S1, ptr addrspace(7) @S2
  store volatile i8 1, ptr addrspace(7) %p, align 1
  ret void
}
