// RUN: %clang -target mcs51 -S -O1 %s -o - | FileCheck %s

// CHECK-LABEL: add_one:
// CHECK: add a, #1
// CHECK: ret
unsigned char add_one(unsigned char value) { return value + 1; }
