// Runtime fixture for verify-arith32-runtime.sh: compares the 32-bit multiply
// and variable-shift runtime helpers with reference loops that only use shifts
// by one, which the compiler expands inline.
// The flat image starts at address zero: set up the hardware stack above the
// register banks before entering main.
__asm__(".section .text.0start,\"ax\"\n"
        "  mov sp, #0x2f\n"
        "  lcall main\n"
        "  sjmp .\n");

__attribute__((section(".mcs51.simresult")))
volatile __xdata unsigned char Result;

static __code const unsigned long Values[] = {
    0ul,          1ul,          0x80000000ul, 0xfffffffful,
    0x12345678ul, 0x0000ff01ul, 0x7ffffffful,  0xdeadbeeful,
};
static __code const unsigned char Counts[] = {0, 1, 7, 8, 9, 15, 16, 17, 24, 31};

static unsigned long shl_ref(unsigned long v, unsigned char n) {
  while (n--)
    v <<= 1;
  return v;
}
static unsigned long shr_ref(unsigned long v, unsigned char n) {
  while (n--)
    v >>= 1;
  return v;
}
static long sar_ref(long v, unsigned char n) {
  while (n--)
    v >>= 1;
  return v;
}
static unsigned long mul_ref(unsigned long a, unsigned long b) {
  unsigned long product = 0;
  for (unsigned char i = 0; i != 32; ++i) {
    if (b & 1)
      product += a;
    a <<= 1;
    b >>= 1;
  }
  return product;
}

int main(void) {
  unsigned char failures = 0;
  for (unsigned char i = 0; i != sizeof Values / sizeof Values[0]; ++i) {
    volatile unsigned long v = Values[i];
    for (unsigned char j = 0; j != sizeof Counts / sizeof Counts[0]; ++j) {
      volatile unsigned char n = Counts[j];
      if ((v << n) != shl_ref(v, n))
        ++failures;
      if ((v >> n) != shr_ref(v, n))
        ++failures;
      if ((long)(((volatile long)v) >> n) != sar_ref((long)v, n))
        ++failures;
    }
    for (unsigned char j = 0; j != sizeof Values / sizeof Values[0]; ++j) {
      volatile unsigned long w = Values[j];
      if (v * w != mul_ref(v, w))
        ++failures;
    }
  }
  Result = failures;
  for (;;) {}
}
