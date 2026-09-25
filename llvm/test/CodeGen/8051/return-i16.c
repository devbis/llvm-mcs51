// RUN: %clang -target mcs51 -S -emit-llvm -O0 %s -o - | llc -mtriple=mcs51 -o - | FileCheck %s

// CHECK-LABEL: return_word:
// CHECK: mov dptr, #4660
// CHECK: ret
unsigned int return_word(void) { return 0x1234; }
