// RUN: clang -target mcs51 -Os -S %s -o - | FileCheck %s

unsigned char rotate_left_one(unsigned char value) {
  return (unsigned char)((value << 1) | (value >> 7));
}

// CHECK-LABEL: rotate_left_one:
// CHECK:       rl a
// CHECK:       ret
