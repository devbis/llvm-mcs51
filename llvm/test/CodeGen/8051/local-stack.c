// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

unsigned char local_byte(unsigned char value) {
  volatile unsigned char slot;
  slot = value;
  return slot;
}

// Locals live in IDATA; they must not be accessed with MOVX.
// CHECK-LABEL: local_byte:
// CHECK: mov @r{{[01]}}, a
// CHECK: mov a, @r{{[01]}}
// CHECK-NOT: movx
// CHECK: ret

unsigned char local_array_element(unsigned char value) {
  volatile unsigned char slot[4];
  slot[2] = value;
  return slot[2];
}

// CHECK-LABEL: local_array_element:
// CHECK: mov @r{{[01]}}, a
// CHECK: mov a, @r{{[01]}}
// CHECK-NOT: movx
// CHECK: ret
