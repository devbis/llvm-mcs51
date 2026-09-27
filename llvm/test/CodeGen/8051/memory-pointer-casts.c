// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

typedef unsigned char byte;

__xdata byte *idata_to_xdata(__idata byte *Pointer) {
  return (__xdata byte *)Pointer;
}

__idata byte *xdata_to_idata(__xdata byte *Pointer) {
  return (__idata byte *)Pointer;
}

// CHECK-LABEL: idata_to_xdata:
// CHECK: mov r0, #0
// CHECK: mov 130, a
// CHECK: mov a, r0
// CHECK: mov 131, a
// CHECK: ret
// CHECK-LABEL: xdata_to_idata:
// CHECK: mov a, 130
// CHECK: ret
