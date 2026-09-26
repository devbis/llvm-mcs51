// RUN: not clang -target mcs51 -fsyntax-only %s 2>&1 | FileCheck %s

void __attribute__((interrupt(0))) handler(void) {}

void call_handler(void) {
  handler();
}

void __attribute__((interrupt(18))) invalid_vector(void) {}

// CHECK: error: MCS-51 interrupt service routine cannot be called directly
// CHECK: error: 'interrupt' attribute parameter 18 is out of bounds
