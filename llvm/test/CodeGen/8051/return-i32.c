// RUN: clang -target mcs51 -S -O0 %s -o - | FileCheck %s

unsigned long return_long(void) { return 0x12345678UL; }
__attribute__((noinline)) unsigned long identity_long(unsigned long value) {
  return value;
}
unsigned long call_long(unsigned long value) { return identity_long(value); }

// CHECK-LABEL: return_long:
// CHECK: mov
// CHECK: ret
// CHECK-LABEL: identity_long:
// CHECK: ret
// CHECK-LABEL: call_long:
// CHECK: lcall identity_long
// CHECK: ret

// RUN: clang -target mcs51 -S -O1 %s -o - | FileCheck %s --check-prefix=OPT
// OPT-LABEL: identity_long:
// OPT: ret
// OPT-LABEL: call_long:
// OPT: ret
