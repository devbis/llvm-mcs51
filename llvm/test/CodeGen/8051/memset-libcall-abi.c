// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s

void fill_default(unsigned char *Destination, int Value, unsigned short Count) {
  __builtin_memset(Destination, Value, Count);
}

// The intrinsic's i8 fill operand must be widened to the runtime's i16 int
// argument. In particular, it must not travel in R7 while Count is shifted
// into IP1; memset reads Value from IP1 and Count from IP2.
// CHECK-LABEL: fill_default:
// CHECK-NOT: mov r7,
// CHECK-NOT: mov 50, 52
// CHECK-NOT: mov 51, 53
// CHECK: lcall __mcs51_bankcall_memset
