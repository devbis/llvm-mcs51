// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

void copy_local_words(void) {
  volatile unsigned short source = 0x1234;
  volatile unsigned short destination;
  destination = source;
}

// CHECK-LABEL: copy_local_words:
// CHECK: mov @r1, a
// CHECK: mov a, @r1
// CHECK: mov 131, a
// CHECK: ret
