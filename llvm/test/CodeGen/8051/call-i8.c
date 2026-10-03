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
extern void callee_mixed_stack(unsigned char, unsigned char, unsigned char,
                               unsigned char, unsigned char, unsigned int,
                               unsigned int);

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
void caller_mixed_stack(unsigned char a, unsigned char b, unsigned char c,
                        unsigned char d) {
  callee_mixed_stack(a, b, c, d, 0x5a, 0, 0x1234);
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
// CHECK: mov 49, #0
// CHECK: ret

// CHECK-LABEL: caller_word_arg:
// CHECK: lcall callee_word_arg
// CHECK: ret

// CHECK-LABEL: echo_word:
// CHECK: ret

// The fifth byte argument is the first one on the stack.
// CHECK-LABEL: fifth_arg:
// CHECK: mov r1, 129
// CHECK: dec r1
// CHECK: dec r1
// CHECK: mov a, @r1
// CHECK: ret

// CHECK-LABEL: caller5:
// CHECK: push
// CHECK: lcall callee5
// CHECK: dec 129
// CHECK: ret

// Word arguments travel in the first imaginary pairs, so the sixth argument
// here is the second pair.
// CHECK-LABEL: sixth_word:
// CHECK: mov 48, 50
// CHECK: mov 49, 51
// CHECK: ret

// CHECK-LABEL: caller6_word:
// CHECK: lcall callee6_word
// CHECK: ret

// CHECK-LABEL: caller_mixed_stack:
// CHECK: push 224
// CHECK: mov 50, #52
// CHECK: mov 51, #18
// CHECK: lcall callee_mixed_stack
// CHECK: dec 129
// CHECK: ret

// RELOC: R_8051_16_BE callee 0x0
// RELOC: R_8051_16_BE callee_word 0x0
// RELOC: R_8051_16_BE callee_word_arg 0x0
// RELOC: R_8051_16_BE callee5 0x0
// RELOC: R_8051_16_BE callee6_word 0x0
