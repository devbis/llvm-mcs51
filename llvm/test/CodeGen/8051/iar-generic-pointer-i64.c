// RUN: clang -target mcs51 -mcpu=cc2530 -mabi=iar -O2 -S %s -o - | FileCheck %s

typedef __generic volatile unsigned long long *GenericQuadPointer;

static __attribute__((noinline)) void
iar_generic_quad_roundtrip_impl(GenericQuadPointer Pointer) {
  unsigned long long Value = *Pointer;
  *Pointer = Value;
}

void iar_generic_quad_roundtrip(GenericQuadPointer Pointer) {
  iar_generic_quad_roundtrip_impl(Pointer);
}

// The IAR scalar return ABI has no 64-bit result convention. Generic quad
// accesses therefore use two supported 32-bit load helpers and two stores.
// CHECK-LABEL: iar_generic_quad_roundtrip_impl:
// CHECK-NOT: __mcs51_gptrget64
// Generic-pointer stores pass each byte with a packed tag/value word. This
// keeps the store ABI inside the two non-overlapping IAR word-register pairs.
// CHECK: lcall __mcs51_gptrget32
// Calls from this private Clang-ABI function still use the IAR runtime ABI;
// preserve the first 32-bit result across the second helper call.
// CHECK-NEXT: mov {{[0-9]+}}, r2
// CHECK-NEXT: mov {{[0-9]+}}, r3
// CHECK-NEXT: mov {{[0-9]+}}, r4
// CHECK-NEXT: mov {{[0-9]+}}, r5
// CHECK: lcall __mcs51_gptrget32
// CHECK-NOT: __mcs51_gptrget64
// CHECK-COUNT-8: lcall __mcs51_gptrput8
// CHECK-NOT: __mcs51_gptrput64
