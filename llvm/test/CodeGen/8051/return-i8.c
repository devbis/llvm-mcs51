// RUN: %clang -target mcs51 -S -O0 %s -o - | FileCheck %s

// CHECK-LABEL: answer:
// CHECK: mov a, #42
// CHECK: ret
unsigned char answer(void) { return 42; }
