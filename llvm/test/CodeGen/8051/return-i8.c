// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -S -O1 %s -o - | FileCheck %s

// CHECK-LABEL: answer:
// CHECK: mov a, #42
// CHECK-NEXT: ret
unsigned char answer(void) { return 42; }

// CHECK-LABEL: false_value:
// CHECK: clr a
// CHECK-NEXT: ret
_Bool false_value(void) { return 0; }

// CHECK-LABEL: negative_value:
// CHECK: mov a, #-1
// CHECK-NEXT: ret
signed char negative_value(void) { return -1; }
