// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s

typedef unsigned char bit_t __attribute__((address_space(6)));
extern volatile bit_t ready;

unsigned char read_ready(void) { return ready; }
void write_ready(unsigned char value) { ready = value; }
void set_ready(void) { ready = 1; }
void clear_ready(void) { ready = 0; }

// CHECK-LABEL: read_ready:
// CHECK: mov c, ready
// CHECK: clr a
// CHECK-NEXT: rlc a
// CHECK-LABEL: write_ready:
// CHECK: mov c, 224
// CHECK-NEXT: mov ready, c
// CHECK-LABEL: set_ready:
// CHECK: setb ready
// CHECK-LABEL: clear_ready:
// CHECK: clr ready
