// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -S %s -o - | FileCheck %s

__xdata volatile unsigned char flags;
__xdata volatile unsigned char sequence;
__xdata volatile unsigned char key_sequence[2];

unsigned char find_slot(void) {
  unsigned char slot;
  for (slot = 0; slot != 2; ++slot)
    if ((flags & (1u << slot)) && key_sequence[slot] == sequence)
      return slot;
  return 2;
}

// The loop bounds the bit index to zero or one. Select the corresponding byte
// mask instead of emitting a variable-shift loop.
// CHECK-LABEL: find_slot:
// CHECK: mov dptr, #flags
// CHECK: movx a, @dptr
// CHECK: anl a, #1
// CHECK: mov dptr, #key_sequence
// CHECK: movx a, @dptr
// CHECK: mov dptr, #flags
// CHECK: movx a, @dptr
// CHECK: anl a, #2
// CHECK: mov dptr, #key_sequence+1
// CHECK-NOT: djnz
// CHECK: .Lfunc_end
