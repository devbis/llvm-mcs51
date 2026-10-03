// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null

// A byte that is only ever 0 or 1 becomes an i1 global; its loads and stores
// access the byte and keep only the low bit.
static __xdata unsigned char busy;
__xdata unsigned short table[8];

void start(void) { busy = 1; }
unsigned char is_busy(void) { return busy; }
void stop(void) { busy = 0; }

// 16-bit store through a computed XDATA address.
void fill(unsigned char index, unsigned short value) { table[index] = value; }

// CHECK-LABEL: is_busy:
// CHECK: movx a, @dptr
// CHECK: anl a, #1
// CHECK: ret
// CHECK-LABEL: fill:
// CHECK: movx @dptr, a
// CHECK: inc dptr
// CHECK: movx @dptr, a
// CHECK: ret
