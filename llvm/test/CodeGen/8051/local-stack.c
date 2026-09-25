// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

unsigned char local_byte(unsigned char value) {
  volatile unsigned char slot;
  slot = value;
  return slot;
}

// CHECK-LABEL: local_byte:
// CHECK: inc 129
// CHECK: mov @r1, a
// CHECK: mov a, @r1
// CHECK: ret
