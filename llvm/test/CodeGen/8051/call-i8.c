// RUN: clang -target mcs51 -S -emit-llvm -O1 %s -o - | llc -mtriple=mcs51 -o - | FileCheck %s
// RUN: clang -target mcs51 -S -emit-llvm -O1 %s -o - | llc -mtriple=mcs51 -filetype=obj -o %t.o
// RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

extern unsigned char callee(unsigned char);
extern unsigned int callee_word(void);
extern unsigned int callee_word_arg(unsigned int);
extern unsigned char callee5(unsigned char, unsigned char, unsigned char,
                             unsigned char, unsigned char);
extern unsigned int callee6_word(unsigned char, unsigned char, unsigned char,
                                 unsigned char, unsigned int, unsigned int);

unsigned char caller(void) { return callee(42); }
unsigned int caller_word(void) { return callee_word(); }
unsigned int caller_promoted(void) { return callee(42); }
unsigned int caller_word_arg(unsigned int value) {
  return callee_word_arg(value);
}
unsigned int echo_word(unsigned int value) { return value; }
unsigned char fifth_arg(unsigned char a, unsigned char b, unsigned char c,
                        unsigned char d, unsigned char e) {
  return e;
}
unsigned char caller5(unsigned char a, unsigned char b, unsigned char c,
                      unsigned char d, unsigned char e) {
  return callee5(a, b, c, d, e);
}
unsigned int sixth_word(unsigned char a, unsigned char b, unsigned char c,
                        unsigned char d, unsigned int e, unsigned int f) {
  return f;
}
unsigned int caller6_word(unsigned char a, unsigned char b, unsigned char c,
                          unsigned char d, unsigned int e, unsigned int f) {
  return callee6_word(a, b, c, d, 0, f);
}

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

// CHECK-LABEL: caller_word_arg:
// CHECK: lcall callee_word_arg
// CHECK: ret

// CHECK-LABEL: echo_word:
// CHECK: ret

// CHECK-LABEL: fifth_arg:
// CHECK: mov a, 129
// CHECK: add a, #-2
// CHECK: mov r0, a
// CHECK: mov a, @r0
// CHECK: ret

// CHECK-LABEL: caller5:
// CHECK: push
// CHECK: lcall callee5
// CHECK: pop
// CHECK: ret

// CHECK-LABEL: sixth_word:
// CHECK: mov a, 129
// CHECK: add a, #-2
// CHECK: mov r0, a
// CHECK: mov a, @r0
// CHECK: mov 130, a
// CHECK: mov a, r0
// CHECK: dec a
// CHECK: mov r0, a
// CHECK: mov a, @r0
// CHECK: mov 131, a
// CHECK: ret

// CHECK-LABEL: caller6_word:
// CHECK: mov a, 129
// CHECK: add a, #-2
// CHECK: mov r0, a
// CHECK: mov a, @r0
// CHECK: mov 130, a
// CHECK: mov a, r0
// CHECK: dec a
// CHECK: mov r0, a
// CHECK: mov a, @r0
// CHECK: mov 131, a
// CHECK: mov a, 131
// CHECK: push 224
// CHECK: mov a, 130
// CHECK: push 224
// CHECK: mov dptr, #0
// CHECK: lcall callee6_word
// CHECK: pop 240
// CHECK: pop 240
// CHECK: ret

// RELOC: R_8051_16 callee 0x0
// RELOC: R_8051_16 callee_word 0x0
// RELOC: R_8051_16 callee_word_arg 0x0
// RELOC: R_8051_16 callee5 0x0
// RELOC: R_8051_16 callee6_word 0x0
