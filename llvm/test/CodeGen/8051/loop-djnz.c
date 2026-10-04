// RUN: clang -target mcs51 -Oz -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -Oz -mllvm -verify-machineinstrs -S %s -o /dev/null

// A countdown loop on a byte is DJNZ, whichever way the optimizer steps the
// counter: nothing else is left in the loop but its body.
void spin_down(void) {
  unsigned char i = 56;
  do {
    __asm__ volatile("nop");
  } while (--i);
}
void spin_arg(unsigned char n) {
  do {
    __asm__ volatile("nop");
  } while (--n);
}

// CHECK-LABEL: spin_down:
// CHECK: mov r0, #56
// CHECK: nop
// CHECK: djnz r0,
// CHECK: ret
// CHECK-LABEL: spin_arg:
// CHECK: nop
// CHECK: djnz r7,
