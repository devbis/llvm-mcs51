// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

unsigned char local_byte(unsigned char value) {
  volatile unsigned char slot;
  slot = value;
  return slot;
}

// CHECK-LABEL: local_byte:
// CHECK: add a, #7
// CHECK: mov @r1, a
// CHECK: mov a, @r1
// CHECK: ret

unsigned char local_array_element(unsigned char value) {
  volatile unsigned char slot[4];
  slot[2] = value;
  return slot[2];
}

// CHECK-LABEL: local_array_element:
// CHECK: mov @r1, a
// CHECK: mov a, @r1
// CHECK: ret
