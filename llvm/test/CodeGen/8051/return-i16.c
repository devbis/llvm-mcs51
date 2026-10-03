// RUN: clang -target mcs51 -S -emit-llvm -O0 %s -o - | llc -mtriple=mcs51 -o - | FileCheck %s

// CHECK-LABEL: return_word:
// CHECK: mov 48, #52
// CHECK: mov 49, #18
// CHECK: ret
unsigned int return_word(void) { return 0x1234; }
