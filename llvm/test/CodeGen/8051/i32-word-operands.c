// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -filetype=obj %t.s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

volatile __xdata unsigned long lhs;
volatile __xdata unsigned long rhs;
volatile __xdata signed long signed_lhs;

unsigned long add_words(void) { return lhs + rhs; }
unsigned long subtract_words(void) { return lhs - rhs; }
unsigned char equals_all_ones(void) { return lhs == 0xffffffffUL; }
unsigned char less_than_word_pair(void) { return lhs < rhs; }
unsigned char less_than_constant(void) { return lhs < 0x01020304UL; }
unsigned char signed_less_than_constant(void) {
  return signed_lhs < 0x01020304L;
}

// Each loaded word goes straight into a pair of direct bytes before the next
// XDATA load reuses DPTR, and the arithmetic then runs over those bytes.
// CHECK-LABEL: add_words:
// CHECK: mov dptr, #lhs+2
// CHECK: mov 48, a
// CHECK: mov 49, a
// CHECK: mov dptr, #lhs
// CHECK: mov 50, a
// CHECK: mov 51, a
// CHECK: mov dptr, #rhs+2
// CHECK: mov 52, a
// CHECK: mov 53, a
// CHECK: mov dptr, #rhs
// CHECK: mov 54, a
// CHECK: mov 55, a
// CHECK: add a, 50
// CHECK-COUNT-3: addc a, {{[0-9]+}}
// CHECK-LABEL: subtract_words:
// CHECK: mov dptr, #lhs+2
// CHECK: mov dptr, #lhs
// CHECK: mov dptr, #rhs+2
// CHECK: mov dptr, #rhs
// CHECK: subb a, 54
// CHECK-COUNT-3: subb a, {{[0-9]+}}
// CHECK-LABEL: equals_all_ones:
// CHECK-COUNT-4: xrl a, #255
// CHECK-LABEL: less_than_word_pair:
// CHECK: lcall __mcs51_xdata_ult32
// CHECK-LABEL: less_than_constant:
// The constant bytes are consumed directly by SUBB without staging the
// constant's words through DPTR or temporary registers.
// CHECK: subb a, #4
// CHECK: subb a, #3
// CHECK: subb a, #2
// CHECK: subb a, #1
// CHECK-LABEL: signed_less_than_constant:
// Signed ordering biases the most significant byte before the final compare.
// CHECK: subb a, #4
// CHECK: subb a, #3
// CHECK: subb a, #2
// CHECK: xrl a, #128
// CHECK: subb a, #129
