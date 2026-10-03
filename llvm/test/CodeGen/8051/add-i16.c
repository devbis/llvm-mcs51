// RUN: clang -target mcs51 -O1 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O1 -mllvm -verify-machineinstrs -S %s -o %t.s
// RUN: llvm-mc -triple=mcs51 -filetype=obj %t.s -o /dev/null

unsigned short add_one16(unsigned short value) { return value + 1; }
unsigned short add_two16(unsigned short value) { return value + 2; }
unsigned short subtract_one16(unsigned short value) { return value - 1; }
unsigned short add_negative_one16(unsigned short value) { return value + 65535; }
short subtract_one_signed16(short value) { return value - 1; }
unsigned short add_25616(unsigned short value) { return value + 256; }
unsigned short add_51216(unsigned short value) { return value + 512; }
unsigned short add_25716(unsigned short value) { return value + 257; }
unsigned short add_51416(unsigned short value) { return value + 514; }
unsigned short add_large16(unsigned short value) { return value + 60000; }

// CHECK-LABEL: add_one16:
// CHECK: inc 48
// CHECK-NEXT: mov a, 48
// CHECK-NEXT: jnz .Lmcs51_word_skip{{[0-9]+}}
// CHECK-NEXT: inc 49
// CHECK: ret
// CHECK-LABEL: add_two16:
// CHECK: mov a, 48
// CHECK-NEXT: add a, #2
// CHECK-NEXT: mov 48, a
// CHECK-NEXT: clr a
// CHECK-NEXT: addc a, 49
// CHECK-NEXT: mov 49, a
// CHECK-NEXT: ret
// CHECK-LABEL: subtract_one16:
// CHECK: dec 48
// CHECK-NEXT: mov a, 48
// CHECK-NEXT: cjne a, #255, .Lmcs51_word_skip{{[0-9]+}}
// CHECK-NEXT: dec 49
// CHECK: ret
// CHECK-LABEL: add_negative_one16:
// CHECK: dec 48
// CHECK-NEXT: mov a, 48
// CHECK-NEXT: cjne a, #255, .Lmcs51_word_skip{{[0-9]+}}
// CHECK-NEXT: dec 49
// CHECK: ret
// CHECK-LABEL: subtract_one_signed16:
// CHECK: dec 48
// CHECK-NEXT: mov a, 48
// CHECK-NEXT: cjne a, #255, .Lmcs51_word_skip{{[0-9]+}}
// CHECK-NEXT: dec 49
// CHECK: ret
// CHECK-LABEL: add_25616:
// CHECK: mov a, 49
// CHECK-NEXT: add a, #1
// CHECK-NEXT: mov 49, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_51216:
// CHECK: mov a, 49
// CHECK-NEXT: add a, #2
// CHECK-NEXT: mov 49, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_25716:
// CHECK: mov a, 48
// CHECK-NEXT: add a, #1
// CHECK-NEXT: mov 48, a
// CHECK-NEXT: mov a, 49
// CHECK-NEXT: addc a, #1
// CHECK-NEXT: mov 49, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_51416:
// CHECK: mov a, 48
// CHECK-NEXT: add a, #2
// CHECK-NEXT: mov 48, a
// CHECK-NEXT: mov a, 49
// CHECK-NEXT: addc a, #2
// CHECK-NEXT: mov 49, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_large16:
// CHECK: mov a, 48
// CHECK-NEXT: add a, #96
// CHECK-NEXT: mov 48, a
// CHECK-NEXT: mov a, 49
// CHECK-NEXT: addc a, #234
// CHECK-NEXT: mov 49, a
// CHECK-NEXT: ret
