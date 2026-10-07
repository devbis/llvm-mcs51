// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S %s -o - | FileCheck %s

volatile unsigned char result;
extern unsigned char read_input(void);

void __attribute__((interrupt(0), noinline)) handler(void) {
  volatile unsigned char frame[4];
  unsigned char a = read_input();
  unsigned char b = read_input();
  frame[0] = a;
  frame[1] = b;
  __asm__ volatile("" : "+r"(a), "+r"(b) : : "r2", "r3");
  result = a + b + frame[0] + frame[1];
}

// The handler's ordinary callee-saved registers are pushed before its
// interrupt save set. The local frame is released first, then the interrupt
// save set is popped, and finally the ordinary callee-saved values are restored.
// CHECK-LABEL: handler:
// CHECK: lcall __mcs51_save_r6
// CHECK: push 2
// CHECK: mov 129, a
// CHECK: dec 129
// CHECK: pop 55
// CHECK: pop 54
// CHECK: lcall __mcs51_restore_r6
// CHECK: reti
