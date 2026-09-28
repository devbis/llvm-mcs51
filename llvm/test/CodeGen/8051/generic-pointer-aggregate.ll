; RUN: llc -mtriple=mcs51 -O0 -verify-machineinstrs %s -o - | FileCheck %s

%Aggregate = type { i8, i16, float, ptr addrspace(4), [2 x i16] }

define i8 @load_generic_aggregate(ptr addrspace(8) %Pointer) {
entry:
  %Value = load %Aggregate, ptr addrspace(8) %Pointer, align 1
  %Byte = extractvalue %Aggregate %Value, 0
  ret i8 %Byte
}

define void @store_generic_aggregate(ptr addrspace(8) %Pointer, i8 %Byte,
                                     i16 %Word, float %Float,
                                     ptr addrspace(4) %DataPointer,
                                     i16 %Array0, i16 %Array1) {
entry:
  %Value0 = insertvalue %Aggregate poison, i8 %Byte, 0
  %Value1 = insertvalue %Aggregate %Value0, i16 %Word, 1
  %Value2 = insertvalue %Aggregate %Value1, float %Float, 2
  %Value3 = insertvalue %Aggregate %Value2, ptr addrspace(4) %DataPointer, 3
  %ArrayValue0 = insertvalue [2 x i16] poison, i16 %Array0, 0
  %ArrayValue1 = insertvalue [2 x i16] %ArrayValue0, i16 %Array1, 1
  %Value4 = insertvalue %Aggregate %Value3, [2 x i16] %ArrayValue1, 4
  store %Aggregate %Value4, ptr addrspace(8) %Pointer, align 1
  ret void
}

; CHECK-LABEL: load_generic_aggregate:
; CHECK: lcall __mcs51_gptrget8
; CHECK: lcall __mcs51_gptrget16
; CHECK: lcall __mcs51_gptrgetf32
; CHECK: lcall __mcs51_gptrget16
; CHECK: lcall __mcs51_gptrget16
; CHECK: lcall __mcs51_gptrget16
; CHECK-LABEL: store_generic_aggregate:
; CHECK: lcall __mcs51_gptrput8
; CHECK: lcall __mcs51_gptrput16
; CHECK: lcall __mcs51_gptrputf32
; CHECK: lcall __mcs51_gptrput16
; CHECK: lcall __mcs51_gptrput16
; CHECK: lcall __mcs51_gptrput16
