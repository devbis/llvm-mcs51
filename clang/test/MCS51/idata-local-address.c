// RUN: %clang -target mcs51 -mcpu=cc2530 -mabi=iar -O1 -S %s -o - | FileCheck %s

// A local that lives in IDATA and escapes must be passed as its XDATA alias:
// CC2530 maps IDATA 0x00-0xFF at XDATA 0x1F00-0x1FFF.

extern void f(unsigned char *);

// CHECK-LABEL: g:
// CHECK: mov r0, #31
void g(void) {
  unsigned char x[4];
  f(x);
}
