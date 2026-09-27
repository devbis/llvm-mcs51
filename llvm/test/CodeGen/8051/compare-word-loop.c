// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o /dev/null

unsigned short scan(unsigned short *values, unsigned short count) {
  for (unsigned short i = 0; i < count; ++i)
    if (values[i] == 255)
      return i;
  return 0;
}

unsigned short scan_multiple(unsigned short *values, unsigned short count) {
  for (unsigned short i = 0; i < count; ++i) {
    if (values[i] == 255)
      return i;
    if (values[i] == 511)
      return i + 1;
  }
  return 0;
}
