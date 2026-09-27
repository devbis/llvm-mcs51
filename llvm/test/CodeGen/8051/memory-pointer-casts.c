// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

typedef unsigned char byte;

__pdata byte *idata_to_pdata(__idata byte *Pointer) {
  return (__pdata byte *)Pointer;
}

__idata byte *pdata_to_idata(__pdata byte *Pointer) {
  return (__idata byte *)Pointer;
}

// CHECK-LABEL: idata_to_pdata:
// CHECK: ret
// CHECK-LABEL: pdata_to_idata:
// CHECK: ret
