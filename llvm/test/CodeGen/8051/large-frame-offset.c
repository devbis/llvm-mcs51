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
// The frame-relative byte address wraps to an 8-bit displacement.
// CHECK: add a, #116
// CHECK: ret
