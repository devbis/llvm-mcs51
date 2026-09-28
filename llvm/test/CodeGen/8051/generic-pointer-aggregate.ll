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

define i8 @load_generic_byte_array(ptr addrspace(8) %Pointer) {
entry:
  %Value = load [15 x i8], ptr addrspace(8) %Pointer, align 1
  %Byte = extractvalue [15 x i8] %Value, 0
  ret i8 %Byte
}

define void @store_generic_byte_array(ptr addrspace(8) %Pointer) {
entry:
  %Value0 = insertvalue [15 x i8] poison, i8 1, 0
  %Value1 = insertvalue [15 x i8] %Value0, i8 2, 1
  %Value2 = insertvalue [15 x i8] %Value1, i8 3, 2
  %Value3 = insertvalue [15 x i8] %Value2, i8 4, 3
  %Value4 = insertvalue [15 x i8] %Value3, i8 5, 4
  %Value5 = insertvalue [15 x i8] %Value4, i8 6, 5
  %Value6 = insertvalue [15 x i8] %Value5, i8 7, 6
  %Value7 = insertvalue [15 x i8] %Value6, i8 8, 7
  %Value8 = insertvalue [15 x i8] %Value7, i8 9, 8
  %Value9 = insertvalue [15 x i8] %Value8, i8 10, 9
  %Value10 = insertvalue [15 x i8] %Value9, i8 11, 10
  %Value11 = insertvalue [15 x i8] %Value10, i8 12, 11
  %Value12 = insertvalue [15 x i8] %Value11, i8 13, 12
  %Value13 = insertvalue [15 x i8] %Value12, i8 14, 13
  %Value14 = insertvalue [15 x i8] %Value13, i8 15, 14
  store [15 x i8] %Value14, ptr addrspace(8) %Pointer, align 1
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
; CHECK-LABEL: load_generic_byte_array:
; CHECK: lcall __mcs51_gptrget64
; CHECK: lcall __mcs51_gptrget32
; CHECK: lcall __mcs51_gptrget16
; CHECK: lcall __mcs51_gptrget8
; CHECK-LABEL: store_generic_byte_array:
; CHECK: lcall __mcs51_gptrput64
; CHECK: lcall __mcs51_gptrput32
; CHECK: lcall __mcs51_gptrput16
; CHECK: lcall __mcs51_gptrput8
