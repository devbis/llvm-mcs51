// RUN: clang -target mcs51 -S -O2 %s -o - | FileCheck %s

unsigned char local_array_index(unsigned char index, unsigned char value) {
  volatile unsigned char slot[4];
  slot[index] = value;
  return slot[index];
}

// CHECK-LABEL: local_array_index:
// CHECK: mov a, 130
// CHECK: add a, r1
// CHECK: mov @r1, a
// CHECK: mov a, @r1
// CHECK: ret
