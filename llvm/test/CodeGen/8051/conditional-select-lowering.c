// RUN: clang -target mcs51 -mcpu=cc2530 -S -Oz %s -o - | FileCheck %s

unsigned char select_equal_zero(unsigned char condition, unsigned char yes,
                                unsigned char no) {
  return condition == 0 ? yes : no;
}

unsigned char select_if_negative(signed char condition,
                                 unsigned char value) {
  return condition < 0 ? value : 0;
}

unsigned char select_mask_bit3(unsigned char condition, unsigned char yes,
                               unsigned char no) {
  return (condition & 8u) ? yes : no;
}

unsigned char select_below_64(unsigned char condition, unsigned char yes,
                              unsigned char no) {
  return condition < 64u ? yes : no;
}

unsigned char conditional_add_bit2(unsigned char condition, unsigned char sum,
                                   unsigned char value) {
  return sum + ((condition & 4u) ? value : 0);
}

// Compare and select are lowered as one branch; the compare result is not
// materialized as a separate 0/1 register value.
// CHECK-LABEL: select_equal_zero:
// CHECK: mov a, r7
// CHECK: jz
// CHECK-NOT: clr a
// CHECK-LABEL: select_if_negative:
// Sign extension of A.7 to a byte mask uses carry arithmetic, not repeated
// rotates.
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK-NOT: rrc a
// CHECK-LABEL: select_mask_bit3:
// CHECK: mov a, r7
// CHECK-NEXT: jnb 227,
// CHECK-NOT: anl a, #8
// CHECK-LABEL: select_below_64:
// CHECK: mov a, r7
// CHECK-NEXT: anl a, #192
// CHECK-NEXT: clr c
// CHECK-NEXT: jz
// CHECK-LABEL: conditional_add_bit2:
// CHECK: jnb 226,
// CHECK-NOT: subb a, #0
