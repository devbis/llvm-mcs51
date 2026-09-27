// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -ffreestanding \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding \
// RUN:   -mllvm -verify-machineinstrs -S %s -o /dev/null

unsigned char dense_switch(unsigned char value) {
  switch (value) {
  case 0: return 31;
  case 1: return 17;
  case 2: return 23;
  case 3: return 41;
  case 4: return 29;
  case 5: return 13;
  case 6: return 37;
  case 7: return 19;
  case 8: return 43;
  case 9: return 11;
  case 10: return 47;
  case 11: return 53;
  case 12: return 59;
  case 13: return 61;
  case 14: return 67;
  case 15: return 71;
  default: return 0;
  }
}
