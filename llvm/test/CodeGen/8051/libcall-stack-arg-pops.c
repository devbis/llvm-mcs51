// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -c %s -o /dev/null

// Pure library calls drop the chain of the call, so the stack argument pops
// must be glued to the call or they vanish and leak four bytes per call.

unsigned long divide(unsigned long a, unsigned long b) { return a / b; }

// CHECK-LABEL: divide:
// CHECK: lcall __udivsi3
// CHECK-COUNT-4: dec 129
// CHECK: ret
