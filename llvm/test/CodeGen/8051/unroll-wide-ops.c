// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -S %s -o - | FileCheck %s

// A fixed-trip loop over 32-bit values must stay rolled: each i32 shift, xor
// and select expands to a long byte sequence, so unrolling eight iterations
// grew this function past 2 KiB.

unsigned long crc;

void crc_byte(unsigned char value) {
  unsigned char i;
  crc ^= value;
  for (i = 0; i < 8; i++)
    crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320UL : 0);
}

// CHECK-LABEL: crc_byte:
// CHECK: in Loop: Header=
// CHECK-NOT: .LBB0_40:
