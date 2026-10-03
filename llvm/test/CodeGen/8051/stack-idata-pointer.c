// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s

static unsigned char read_idata(__idata const unsigned char *Pointer) {
  return *Pointer;
}

volatile unsigned char Sink;

void read_local_array(unsigned char Index) {
  unsigned char Values[2] = {0x31, 0x42};
  Sink = read_idata((__idata const unsigned char *)&Values[Index]);
}

// The local array is initialized and indexed in IDATA: the element address is
// the frame address plus the index, passed on as a byte-sized pointer.
// CHECK-LABEL: <read_local_array>:
// CHECK: mov 48, #49
// CHECK: mov 49, #66
// CHECK: mov a, 129
// CHECK: add a, #253
// CHECK: mov r0, a
// CHECK-NOT: movx
// CHECK: add a, r7
// CHECK: mov r7, a
// CHECK: lcall
