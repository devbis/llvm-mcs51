// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s

// CHECK-LABEL: echo:
// CHECK: mov a, r7
// CHECK: ret
unsigned char echo(unsigned char value) { return value; }
