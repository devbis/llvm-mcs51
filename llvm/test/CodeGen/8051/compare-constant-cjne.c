// RUN: clang -target mcs51 -Oz -S %s -o - | FileCheck %s

unsigned char not_equal_value(unsigned char value) {
  return value != 0x2a;
}

// CHECK-LABEL: not_equal_value:
// CHECK: cjne r{{[0-7]}}, #42,
// CHECK-NOT: xrl a, #42
