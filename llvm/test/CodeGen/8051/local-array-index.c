// RUN: clang -target mcs51 -S -O2 %s -o - | FileCheck %s

unsigned char local_array_index(unsigned char index, unsigned char value) {
  volatile unsigned char slot[4];
  slot[index] = value;
  return slot[index];
}

// CHECK-LABEL: local_array_index:
// CHECK: mov a, 129
// CHECK: add a, #4
// CHECK: mov 129, a
// CHECK: add a, r7
// CHECK: mov r0, a
// CHECK: mov a, r6
// CHECK: mov @r0, a
// CHECK: mov a, @r0
// CHECK: ret
