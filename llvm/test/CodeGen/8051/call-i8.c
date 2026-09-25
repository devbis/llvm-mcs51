// RUN: clang -target mcs51 -S -emit-llvm -O1 %s -o - | llc -mtriple=mcs51 -o - | FileCheck %s
// RUN: clang -target mcs51 -S -emit-llvm -O1 %s -o - | llc -mtriple=mcs51 -filetype=obj -o %t.o
// RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

extern unsigned char callee(unsigned char);
extern unsigned int callee_word(void);

unsigned char caller(void) { return callee(42); }
unsigned int caller_word(void) { return callee_word(); }
unsigned int caller_promoted(void) { return callee(42); }

// CHECK-LABEL: caller:
// CHECK: mov a, #42
// CHECK: mov r7, a
// CHECK: lcall callee
// CHECK: ret

// CHECK-LABEL: caller_word:
// CHECK: lcall callee_word
// CHECK: ret

// CHECK-LABEL: caller_promoted:
// CHECK: lcall callee
// CHECK: mov 131, #0
// CHECK: mov 130, a
// CHECK: ret

// RELOC: R_8051_16 callee 0x0
// RELOC: R_8051_16 callee_word 0x0
