// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null

int float_equal(float lhs, float rhs) { return lhs == rhs; }

int float_not_equal(float lhs, float rhs) { return lhs != rhs; }

int float_greater_equal(float lhs, float rhs) { return lhs >= rhs; }

int float_less(float lhs, float rhs) { return lhs < rhs; }

int float_less_equal(float lhs, float rhs) { return lhs <= rhs; }

int float_greater(float lhs, float rhs) { return lhs > rhs; }

// CHECK-LABEL: float_equal:
// CHECK: lcall __eqsf2
// CHECK-LABEL: float_not_equal:
// CHECK: lcall __nesf2
// CHECK-LABEL: float_greater_equal:
// CHECK: lcall __gesf2
// CHECK-LABEL: float_less:
// CHECK: lcall __ltsf2
// CHECK-LABEL: float_less_equal:
// CHECK: lcall __lesf2
// CHECK-LABEL: float_greater:
// CHECK: lcall __gtsf2
