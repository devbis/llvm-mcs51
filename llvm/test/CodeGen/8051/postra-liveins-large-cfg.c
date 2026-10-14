// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -mllvm -mcs51-unroll-threshold=1024 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s

volatile unsigned char Source[64];
volatile unsigned char Destination[64];

void livein_stress(void) {
  for (unsigned short I = 0; I != 64; ++I) {
    unsigned char Value = Source[I];
    if (Value & 1)
      Destination[I] = Value + 3;
    else
      Destination[I] = Value ^ 0x55;
  }
}

// A fully unrolled fixed-trip loop with a conditional path in each iteration
// stresses post-RA live-in propagation over a long block sequence.
// CHECK-LABEL: livein_stress:
// CHECK: ret
