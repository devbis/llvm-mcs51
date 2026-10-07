// Location lists for variables spilled to the stack refer to SP, which has no
// DWARF register number and belongs to no register class. Emitting them must
// not crash.
// RUN: clang -target mcs51 -O2 -g -S %s -o - | FileCheck %s

// CHECK-LABEL: f:
// CHECK: .section .debug_info

extern volatile unsigned char R[64];
unsigned char g(unsigned char);

void f(void) {
  unsigned char n = 0, a = 1, b = 2, c = 3, d = 4, e = 5, k = 6, m = 7, q = 8,
                r = 9;
  for (unsigned char i = 0; i < 10; i++) {
    R[n++] = g(a) + b;
    a = g(c) ^ d;
    b = g(e) + k;
    c = g(m) - q;
    d = g(r) + a;
    e = g(b) * c;
    k = g(d) | e;
    m = g(k) & a;
    q = g(m) + b;
    r = g(q) + n;
  }
  R[n] = a + b + c + d + e + k + m + q + r;
}
