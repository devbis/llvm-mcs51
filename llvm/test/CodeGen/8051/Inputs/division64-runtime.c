// Runtime fixture for verify-div64-runtime.sh. The expected results are
// constants so the reference does not call the division helpers under test.
__asm__(".section .text.0start,\"ax\"\n"
        "  mov sp, #0x2f\n"
        "  lcall main\n"
        "  sjmp .\n");

__attribute__((section(".mcs51.simresult")))
volatile __xdata unsigned char Result;

extern unsigned long long __udivdi3(unsigned long long, unsigned long long);
extern unsigned long long __umoddi3(unsigned long long, unsigned long long);
extern long long __moddi3(long long, long long);

static unsigned char check_unsigned(unsigned long long Numerator,
                                    unsigned long long Denominator,
                                    unsigned long long Quotient,
                                    unsigned long long Remainder) {
  return __udivdi3(Numerator, Denominator) == Quotient &&
         __umoddi3(Numerator, Denominator) == Remainder;
}

int main(void) {
  unsigned char Failures = 0;
  Failures += !check_unsigned(0x123456789abcdef0ULL, 0x0000000100000003ULL,
                              0x0000000012345678ULL, 0x00000000641fdb88ULL);
  Failures += !check_unsigned(0xffffffffffffffffULL, 0x0000000100000001ULL,
                              0x00000000ffffffffULL, 0);
  Failures += !check_unsigned(0x8000000000000000ULL, 0x7fffffffffffffffULL,
                              1, 1);
  Failures += !check_unsigned(0xdeadbeefcafebabeULL, 0x0000000000012345ULL,
                              0x0000c3b6fa62cc6bULL, 0xe7);
  Failures += !check_unsigned(0x0123456789abcdefULL, 17,
                              0x0011223344556677ULL, 8);

  Failures += __moddi3(-1234567890123456789LL, 987654321LL) !=
              -725308641LL;
  Failures += __moddi3((-9223372036854775807LL - 1), 3) != -2;
  Failures += __moddi3(9223372036854775807LL, -17) != 8;
  Failures += __moddi3(-7, 3) != -1;

  Result = Failures;
  for (;;) {
  }
}
