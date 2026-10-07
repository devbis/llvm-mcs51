// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -c %s -o /dev/null

#define SP (*(volatile __sfr unsigned char *)0x81)

__attribute__((noinline)) unsigned char write_sp_from_idata(
    __idata volatile unsigned char *source, unsigned char value) {
  volatile unsigned char slots[8];
  slots[0] = value;
  unsigned char before = slots[0];
  SP = *source;
  slots[7] = before;
  return slots[7];
}

// The SFR store writes SP. Stack addresses used after it must be based on the
// current SP, rather than adjusted from a pre-store cached address.
// CHECK-LABEL: write_sp_from_idata:
// CHECK:       mov a, 129
// CHECK:       mov -127, a
// CHECK:       mov a, 129
// CHECK:       add a, #{{[0-9]+}}
