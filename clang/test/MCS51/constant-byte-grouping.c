// RUN: %clang_cc1 -triple mcs51 -target-cpu cc2530 -Oz -S %s -o - | FileCheck %s

// Constant byte grouping must not dereference the block end when the scan for
// stores of one constant reaches the end of the block (an assertion failure in
// builds with assertions, undefined behaviour otherwise).

// CHECK-LABEL: r0:
// CHECK:       clr a
// CHECK-NEXT:  mov 48, a
// CHECK-NEXT:  mov 49, a
// CHECK-NEXT:  mov 50, a
// CHECK-NEXT:  mov 51, a
// CHECK-NEXT:  ret
unsigned long r0(void) { return 0; }

// CHECK-LABEL: r1:
// CHECK:       mov a, #1
// CHECK-NEXT:  mov 48, a
// CHECK-NEXT:  mov 49, a
// CHECK-NEXT:  mov 50, a
// CHECK-NEXT:  mov 51, a
// CHECK-NEXT:  ret
unsigned long r1(void) { return 0x01010101UL; }
