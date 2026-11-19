// An 8-bit PDATA/IDATA address whose constant offset leaves the object (here
// pb[k - 5], reassociated to (pb - 5) + k) must not be folded into the
// range-checked R_8051_8 immediate: with pb at PDATA 0, "#pb-5" cannot be
// linked. Out-of-object offsets are added as a separate modulo-256 immediate;
// in-object offsets are still folded.
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -Os -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK

typedef unsigned char u8;

volatile __pdata u8 pb[3] = {9, 8, 7};
volatile __idata u8 ib[4] = {1, 2, 3, 4};
volatile __xdata u8 out;

__attribute__((noinline)) void below_pdata(u8 k) { out = pb[k - 5]; }
__attribute__((noinline)) void below_idata(u8 k) { out = ib[k - 7]; }
__attribute__((noinline)) void inside_idata(u8 k) { out = ib[k + 2]; }

int main(void) {
  below_pdata(6);
  below_idata(8);
  inside_idata(1);
  return 0;
}

// CHECK-LABEL: below_pdata:
// CHECK-NOT: #pb-
// CHECK: mov r{{[0-7]}}, #pb{{$}}
// CHECK: add a, #-5
// CHECK-LABEL: below_idata:
// CHECK-NOT: #ib-
// CHECK: mov r{{[0-7]}}, #ib{{$}}
// CHECK: add a, #-7
// CHECK-LABEL: inside_idata:
// CHECK: mov r{{[0-7]}}, #ib+2

// LINK: Name: pb
// LINK-NEXT: Value: 0x0
