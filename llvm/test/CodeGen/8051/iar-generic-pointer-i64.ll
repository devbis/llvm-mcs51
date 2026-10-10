; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -O2 %s -o - | FileCheck %s

target triple = "mcs51"

define void @iar_generic_aggregate_i64(ptr addrspace(8) %pointer) #0 {
  %value = load volatile { i64 }, ptr addrspace(8) %pointer, align 1
  store volatile { i64 } %value, ptr addrspace(8) %pointer, align 1
  ret void
}

define internal void @iar_private_generic_aggregate_i64(
    ptr addrspace(8) %pointer) #1 {
  %value = load volatile i64, ptr addrspace(8) %pointer, align 1
  store volatile i64 %value, ptr addrspace(8) %pointer, align 1
  ret void
}

define void @iar_generic_private_aggregate_i64(ptr addrspace(8) %pointer) #0 {
  call void @iar_private_generic_aggregate_i64(ptr addrspace(8) %pointer)
  ret void
}

attributes #0 = { "mcs51-abi"="iar" }
attributes #1 = { "mcs51-abi"="clang" }

; The scalar helpers have 32-bit results, so this aggregate field avoids
; introducing a helper call that returns an unsupported 64-bit IAR value.
; CHECK-LABEL: iar_generic_aggregate_i64:
; CHECK-NOT: __mcs51_gptrget64
; CHECK: lcall __mcs51_gptrget32
; CHECK: lcall __mcs51_gptrget32
; CHECK-NOT: __mcs51_gptrget64
; CHECK: lcall __mcs51_gptrput32
; CHECK: lcall __mcs51_gptrput32
; CHECK-NOT: __mcs51_gptrput64

; A private helper in an IAR module uses Clang's local call ABI, but calls the
; IAR generic-pointer runtime. Keep its i64 accesses split into 32-bit calls.
; CHECK-LABEL: iar_private_generic_aggregate_i64:
; CHECK-NOT: __mcs51_gptrget64
; CHECK: lcall __mcs51_gptrget32
; CHECK: lcall __mcs51_gptrget32
; CHECK-NOT: __mcs51_gptrget64
; CHECK: lcall __mcs51_gptrput32
; CHECK: lcall __mcs51_gptrput32
; CHECK-NOT: __mcs51_gptrput64
