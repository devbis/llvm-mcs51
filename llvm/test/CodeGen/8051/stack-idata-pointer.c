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

// CHECK-LABEL: <read_local_array>:
// CHECK: mov a, 129
// CHECK: add a, #255
// CHECK: mov r1, a
// CHECK: add a, r{{[0-7]}}
// CHECK: mov r7, a
