// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -c %s -o /dev/null

// Pure library calls drop the chain of the call, so the stack argument pops
// must be glued to the call or they vanish and leak the arguments on every
// call. 64-bit operands are passed on the stack.

unsigned long long divide(unsigned long long a, unsigned long long b) {
  return a / b;
}

// CHECK-LABEL: divide:
// CHECK: lcall __udivdi3
// CHECK-COUNT-16: dec 129
// CHECK: ret
