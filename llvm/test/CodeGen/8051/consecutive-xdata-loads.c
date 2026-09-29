// RUN: clang -target mcs51 -mcpu=cc2530 -S -O1 %s -o - | FileCheck %s

__xdata volatile unsigned char bytes[3];

unsigned char read_bytes(void) {
  unsigned char a = bytes[0];
  unsigned char b = bytes[1];
  unsigned char c = bytes[2];
  return (unsigned char)(a + b + c);
}

// Consecutive volatile XDATA reads can share the address in DPTR.
// CHECK-LABEL: read_bytes:
// CHECK: mov dptr, #bytes
// CHECK-NEXT: movx a, @dptr
// CHECK: inc dptr
// CHECK: movx a, @dptr
// CHECK: inc dptr
// CHECK: movx a, @dptr
