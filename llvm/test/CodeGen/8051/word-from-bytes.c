// RUN: clang -target mcs51 -Oz -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o /dev/null

// A word built from two bytes is the pair of those bytes: no shifts or ORs.
unsigned short make(unsigned char hi, unsigned char lo) {
  return ((unsigned short)hi << 8) | lo;
}
unsigned short make_add(unsigned char hi, unsigned char lo) {
  return ((unsigned short)hi << 8) + lo;
}
unsigned long make4(unsigned char a, unsigned char b, unsigned char c,
                    unsigned char d) {
  return (unsigned long)a << 24 | (unsigned long)b << 16 |
         (unsigned long)c << 8 | d;
}

// CHECK-LABEL: make:
// CHECK-NEXT: ; %bb.0:
// CHECK-NEXT: mov 48, r6
// CHECK-NEXT: mov 49, r7
// CHECK-NEXT: ret
// CHECK-LABEL: make_add:
// CHECK-NEXT: ; %bb.0:
// CHECK-NEXT: mov 48, r6
// CHECK-NEXT: mov 49, r7
// CHECK-NEXT: ret
// CHECK-LABEL: make4:
// CHECK-NOT: orl
// CHECK: ret
