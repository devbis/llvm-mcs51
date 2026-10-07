// Runtime fixture for verify-div64-runtime.sh. The expected results are
// constants so the reference does not call the division helpers under test.
__asm__(".section .text.0start,\"ax\"\n"
        "  mov sp, #0x7f\n"
        "  lcall main\n"
        "  sjmp .\n");

struct DivisionResult {
  unsigned char Code;
  unsigned long long Observed;
  unsigned long long Expected;
};
__attribute__((section(".mcs51.simresult")))
volatile __xdata struct DivisionResult Result;
static volatile __xdata unsigned long long InputNumerator;
static volatile __xdata unsigned long long InputDenominator;
static volatile __xdata long long SignedNumerator;
static volatile __xdata long long SignedDenominator;

#define CHECK_UNSIGNED(N, D, Q, R, CODE)                                    \
  do {                                                                       \
    InputNumerator = (N);                                                    \
    InputDenominator = (D);                                                  \
    unsigned long long Actual = InputNumerator / InputDenominator;           \
    Result.Observed = Actual;                                                \
    Result.Expected = (Q);                                                  \
    if (Actual != (Q)) {                                                     \
      Failure = (CODE);                                                      \
      goto done;                                                             \
    }                                                                        \
    Actual = InputNumerator % InputDenominator;                              \
    Result.Observed = Actual;                                                \
    Result.Expected = (R);                                                  \
    if (Actual != (R)) {                                                     \
      Failure = (CODE) + 1;                                                  \
      goto done;                                                             \
    }                                                                        \
  } while (0)

#define CHECK_SIGNED_REMAINDER(N, D, R, CODE)                               \
  do {                                                                       \
    SignedNumerator = (N);                                                   \
    SignedDenominator = (D);                                                 \
    long long Actual = SignedNumerator % SignedDenominator;                  \
    Result.Observed = (unsigned long long)Actual;                            \
    Result.Expected = (unsigned long long)(R);                              \
    if (Actual != (R)) {                                                     \
      Failure = (CODE);                                                      \
      goto done;                                                             \
    }                                                                        \
  } while (0)

int main(void) {
  unsigned char Failure = 0;
  CHECK_UNSIGNED(0x123456789abcdef0ULL, 0x0000000100000003ULL,
                 0x0000000012345678ULL, 0x00000000641fdb88ULL, 1);
  CHECK_UNSIGNED(0xffffffffffffffffULL, 0x0000000100000001ULL,
                 0x00000000ffffffffULL, 0, 3);
  CHECK_UNSIGNED(0x8000000000000000ULL, 0x7fffffffffffffffULL, 1, 1, 5);
  CHECK_UNSIGNED(0xdeadbeefcafebabeULL, 0x0000000000012345ULL,
                 0x0000c3b6fa62cc6bULL, 0xe7, 7);
  CHECK_UNSIGNED(0x0123456789abcdefULL, 17, 0x0011223344556677ULL, 8, 9);
  CHECK_SIGNED_REMAINDER(-1234567890123456789LL, 987654321LL, -725308641LL,
                         11);
  CHECK_SIGNED_REMAINDER((-9223372036854775807LL - 1), 3, -2, 12);
  CHECK_SIGNED_REMAINDER(9223372036854775807LL, -17, 8, 13);
  CHECK_SIGNED_REMAINDER(-7, 3, -1, 14);
done:
  Result.Code = Failure;
  for (;;) {
  }
}

#undef CHECK_UNSIGNED
#undef CHECK_SIGNED_REMAINDER
