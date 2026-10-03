// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -c %s -o /dev/null

// A 32-bit call result stored to XDATA goes straight from the return
// registers to memory. Routing the bytes through DPTR lost them.

volatile unsigned long Sink;
unsigned long produce(void);

void store_result(void) { Sink = produce(); }

// CHECK-LABEL: store_result:
// CHECK: lcall produce
// CHECK-NOT: mov a, 13
// CHECK-NOT: mov a, 13
// CHECK-COUNT-4: movx @dptr, a
