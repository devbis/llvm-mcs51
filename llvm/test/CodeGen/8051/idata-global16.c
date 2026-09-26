// RUN: clang -target mcs51 -S %s -o - | FileCheck %s

typedef volatile unsigned short idata16
    __attribute__((address_space(2)));

idata16 shared_word = 0x1234;

unsigned short load_word(void) { return shared_word; }
void store_word(unsigned short value) { shared_word = value; }

// CHECK-LABEL: load_word:
// CHECK: mov r0, #shared_word
// CHECK: mov a, @r0
// CHECK: mov 130, a
// CHECK: inc r0
// CHECK: mov a, @r0
// CHECK: mov 131, a
// CHECK-LABEL: store_word:
// CHECK: mov a, 130
// CHECK: mov r0, #shared_word
// CHECK: mov @r0, a
// CHECK: inc r0
// CHECK: mov a, 131
// CHECK: mov @r0, a
