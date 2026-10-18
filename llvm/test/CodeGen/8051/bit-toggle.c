// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s

#define SBIT(address) (*(volatile __sbit unsigned char *)(address))
#define P30 SBIT(0xB0)

__attribute__((noinline)) void toggle_not(void) { P30 = !P30; }
__attribute__((noinline)) void toggle_xor(void) { P30 ^= 1; }

// For !bit, the zero path stores 1 and the nonzero path stores 0. The final
// MOV C, ACC.0 transfers that computed value to the bit-addressed store.
// CHECK-LABEL: toggle_not:
// CHECK: mov c, -80
// CHECK: clr a
// CHECK: rlc a
// CHECK: cjne {{r[0-7]}}, #0, [[NONZERO:.+]]
// CHECK: mov a, #1
// CHECK: sjmp [[STORE:.+]]
// CHECK: [[NONZERO]]:
// CHECK: clr a
// CHECK: [[STORE]]:
// CHECK: mov c, 224
// CHECK-NEXT: mov -80, c

// CHECK-LABEL: toggle_xor:
// CHECK: mov c, -80
// CHECK: clr a
// CHECK: rlc a
// CHECK: xrl a, #1
// CHECK-NEXT: mov c, 224
// CHECK-NEXT: mov -80, c
