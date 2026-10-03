// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

void copy_local_words(void) {
  volatile unsigned short source = 0x1234;
  volatile unsigned short destination;
  destination = source;
}

// CHECK-LABEL: copy_local_words:
// The word locals are written and read through DPTR; the frame addresses are
// kept in register pairs instead of being spilled to the stack.
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: movx a, @dptr
// CHECK: inc dptr
// CHECK: movx a, @dptr
// CHECK: ret
