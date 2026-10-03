// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

void copy_local_words(void) {
  volatile unsigned short source = 0x1234;
  volatile unsigned short destination;
  destination = source;
}

// CHECK-LABEL: copy_local_words:
// Word locals live in IDATA and are accessed through R0, never with MOVX.
// CHECK-NOT: movx
// CHECK: mov @r0, a
// CHECK: mov a, @r0
// CHECK-NOT: movx
// CHECK: ret
