; RUN: llc -mtriple=mcs51 -o - %s | FileCheck %s
; RUN: llc -mtriple=mcs51 -verify-machineinstrs -o /dev/null %s

define i16 @load_selected_generic(i1 %choose_local, ptr %external) {
entry:
  %local = alloca i16, align 2
  store i16 4660, ptr %local, align 2
  %selected = select i1 %choose_local, ptr %local, ptr %external
  %generic = addrspacecast ptr %selected to ptr addrspace(8)
  %value = load volatile i16, ptr addrspace(8) %generic, align 2
  ret i16 %value
}

define i16 @load_phi_generic(i1 %choose_local, ptr %external) {
entry:
  %local = alloca i16, align 2
  store i16 4660, ptr %local, align 2
  br i1 %choose_local, label %local.path, label %external.path

local.path:
  br label %merge

external.path:
  br label %merge

merge:
  %selected = phi ptr [ %local, %local.path ], [ %external, %external.path ]
  %generic = addrspacecast ptr %selected to ptr addrspace(8)
  %value = load volatile i16, ptr addrspace(8) %generic, align 2
  ret i16 %value
}

; CHECK-LABEL: load_selected_generic:
; CHECK: lcall __mcs51_gptrget16
; CHECK-LABEL: load_phi_generic:
; CHECK: lcall __mcs51_gptrget16
