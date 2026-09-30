// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

unsigned char equal_zero(unsigned short value) { return value == 0; }

unsigned char branch_on_zero(unsigned short value) {
  if (value == 0)
    return 3;
  return 7;
}


// CHECK-LABEL: equal_zero:
// CHECK: mov a,
// CHECK-NOT: xrl a, #0
// CHECK: jnz
// CHECK: mov a,
// CHECK-NOT: xrl a, #0
// CHECK: jnz

// CHECK-LABEL: branch_on_zero:
// CHECK: mov a,
// CHECK-NOT: xrl a, #0
// CHECK: jnz
// CHECK: mov a,
// CHECK-NOT: xrl a, #0
// CHECK: jnz
