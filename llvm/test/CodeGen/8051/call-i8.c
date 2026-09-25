// RUN: clang -target mcs51 -S -emit-llvm -O1 %s -o - | llc -mtriple=mcs51 -o - | FileCheck %s
// RUN: clang -target mcs51 -S -emit-llvm -O1 %s -o - | llc -mtriple=mcs51 -filetype=obj -o %t.o
// RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

extern unsigned int callee(unsigned char);

unsigned int caller(void) { return callee(42); }

// CHECK-LABEL: caller:
// CHECK: mov a, #42
// CHECK: mov r7, a
// CHECK: lcall callee
// CHECK: ret

// RELOC: R_8051_16 callee 0x0
