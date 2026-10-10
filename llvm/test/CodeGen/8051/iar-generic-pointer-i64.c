// RUN: clang -target mcs51 -mcpu=cc2530 -mabi=iar -O2 -S %s -o - | FileCheck %s

typedef __generic volatile unsigned long long *GenericQuadPointer;

void iar_generic_quad_roundtrip(GenericQuadPointer Pointer) {
  unsigned long long Value = *Pointer;
  *Pointer = Value;
}

// The IAR scalar return ABI has no 64-bit result convention. Generic quad
// accesses therefore use two supported 32-bit load helpers and two stores.
// CHECK-LABEL: iar_generic_quad_roundtrip:
// CHECK-NOT: __mcs51_gptrget64
// CHECK: lcall __mcs51_gptrget32
// CHECK: lcall __mcs51_gptrget32
// CHECK-NOT: __mcs51_gptrget64
// CHECK: lcall __mcs51_gptrput32
// CHECK: lcall __mcs51_gptrput32
// CHECK-NOT: __mcs51_gptrput64
