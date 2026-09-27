// RUN: clang -target mcs51 -S -O1 -mllvm -verify-machineinstrs %s -o - | FileCheck %s

unsigned char general_register(unsigned char Value) {
  __asm__ volatile ("inc %0" : "+r"(Value));
  return Value;
}

// CHECK-LABEL: general_register:
// CHECK: ;APP
// CHECK-NEXT: inc r{{[0-7]}}
// CHECK-NEXT: ;NO_APP
