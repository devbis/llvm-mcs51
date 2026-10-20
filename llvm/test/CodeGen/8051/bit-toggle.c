// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s

#define SBIT(address) (*(volatile __sbit unsigned char *)(address))
#define P30 SBIT(0xB0)

__attribute__((noinline)) void toggle_not(void) { P30 = !P30; }
__attribute__((noinline)) void toggle_xor(void) { P30 ^= 1; }

// Both compound XOR and logical not of a bit-addressed value are a CPL bit.
// CHECK-LABEL: toggle_not:
// CHECK: cpl -80
// CHECK-NOT: mov c, -80

// CHECK-LABEL: toggle_xor:
// CHECK: cpl -80
// CHECK-NOT: mov c, -80
