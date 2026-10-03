// RUN: clang -target mcs51 -O0 -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -filetype=obj %t.s -o /dev/null

unsigned char large_frame_offset(unsigned char value) {
  volatile unsigned char bytes[140];
  bytes[0] = value;
  bytes[139] = value;
  return bytes[139];
}

// CHECK-LABEL: large_frame_offset:
// Larger stack adjustments use fixed-size SFR sequences.
// CHECK: mov a, 129
// CHECK-NEXT: add a, #141
// CHECK-NEXT: mov 129, a
// The frame-relative byte addresses wrap to an 8-bit displacement.
// CHECK: add a, #116
// CHECK: add a, #117
// Constant offsets that do not fit INC DPTR are added through A.
// CHECK: add a, #139
// CHECK: ret
