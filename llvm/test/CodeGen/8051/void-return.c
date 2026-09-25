// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

// CHECK-LABEL: empty:
// CHECK: ret
void empty(void) {}
