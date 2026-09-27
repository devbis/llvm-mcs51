// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

static unsigned char read_idata(__idata const unsigned char *Pointer) {
  return *Pointer;
}

volatile unsigned char Sink;

void read_local_array(void) {
  unsigned char Values[2] = {0x31, 0x42};
  Sink = read_idata((__idata const unsigned char *)&Values[1]);
}

// CHECK-LABEL: read_local_array:
// CHECK: mov a, 129
// CHECK: add a, #0
// CHECK: mov r1, a
// CHECK: mov r7, a
// CHECK: lcall __mcs51_bankcall_read_idata
