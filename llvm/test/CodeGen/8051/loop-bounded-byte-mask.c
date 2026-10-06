// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib \
// RUN:   -mllvm -enable-machine-outliner=never \
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

// The loop bounds the bit index to zero or one. Test the selected ACC bit
// directly instead of materializing a mask or emitting a variable-shift loop.
// CHECK-LABEL: find_slot:
// CHECK: mov dptr, #xdata16(flags)
// CHECK: movx a, @dptr
// CHECK: jnb 224,
// CHECK: mov dptr, #xdata16(key_sequence)
// CHECK: movx a, @dptr
// CHECK: mov dptr, #xdata16(flags)
// CHECK: movx a, @dptr
// CHECK: jnb 225,
// CHECK: mov dptr, #xdata16(key_sequence+1)
// CHECK-NOT: djnz
// CHECK: .Lfunc_end
