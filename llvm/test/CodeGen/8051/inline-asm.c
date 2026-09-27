// RUN: clang -target mcs51 -S -O1 -mllvm -verify-machineinstrs %s -o - | FileCheck %s

unsigned char general_register(unsigned char Value) {
  __asm__ volatile ("inc %0" : "+r"(Value));
  return Value;
}

unsigned char fixed_register(unsigned char Value) {
  register unsigned char Fixed asm("r0") = Value;
  __asm__ volatile ("inc %0" : "+r"(Fixed));
  return Fixed;
}

void xdata_memory_operand(unsigned char *Pointer) {
  __asm__ volatile ("movx a, %0" : : "m"(*Pointer));
}

// CHECK-LABEL: general_register:
// CHECK: ;APP
// CHECK-NEXT: inc r{{[0-7]}}
// CHECK-NEXT: ;NO_APP
// CHECK-LABEL: fixed_register:
// CHECK: inc r0
// CHECK-LABEL: xdata_memory_operand:
// CHECK: movx a, @dptr
