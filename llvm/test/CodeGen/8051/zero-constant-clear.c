// RUN: clang -target mcs51 -Oz -S %s -o - | FileCheck %s

unsigned char return_zero(void) { return 0; }

// CHECK-LABEL: return_zero:
// CHECK: clr a
// CHECK-NEXT: ret
// CHECK-NOT: mov a, #0
