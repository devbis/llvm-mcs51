// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s

void fill_xdata(__xdata unsigned char *Destination, unsigned char Value,
                unsigned short Count) {
  __builtin_memset(Destination, Value, Count);
}

void fill_xdata_fixed(__xdata unsigned char *Destination, unsigned char Value) {
  __builtin_memset(Destination, Value, 3);
}

// Both dynamic and fixed-size intrinsics retain the XDATA bus and lower to
// ordinary XDATA stores instead of failing in SelectionDAG's generic memset
// expansion or calling the default-space runtime entry point.
// CHECK-LABEL: fill_xdata:
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memset
// CHECK-LABEL: fill_xdata_fixed:
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memset
