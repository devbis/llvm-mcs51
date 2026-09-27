// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null

float add_float(float lhs, float rhs) { return lhs + rhs; }

float subtract_float(float lhs, float rhs) { return lhs - rhs; }

float multiply_float(float lhs, float rhs) { return lhs * rhs; }

float divide_float(float lhs, float rhs) { return lhs / rhs; }

// CHECK-LABEL: add_float:
// CHECK: lcall __addsf3
// CHECK-LABEL: subtract_float:
// CHECK: lcall __subsf3
// CHECK-LABEL: multiply_float:
// CHECK: lcall __mulsf3
// CHECK-LABEL: divide_float:
// CHECK: lcall __divsf3
