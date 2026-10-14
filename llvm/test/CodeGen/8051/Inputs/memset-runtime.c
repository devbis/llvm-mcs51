// Runtime fixture for verify-memset-runtime.sh.
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

__asm__(".section .text.0start,\"ax\"\n"
        "  mov sp, #0x2f\n"
        "  lcall main\n"
        "  sjmp .\n");

__attribute__((
    section(".mcs51.xdata.bss.simresult"))) volatile __xdata uint8_t Result;

uint8_t Buffer[260] __attribute__((section(".mcs51.xdata.bss.memset_buffer")));
__xdata uint8_t ExplicitXData[8];

static __code const uint16_t Lengths[] = {0, 1, 2, 3, 16, 31, 127, 255, 256};
static __code const uint16_t Values[] = {0x0000, 0x00ff, 0x00a5, 0x003c,
                                         0x1234};

extern void *memset(void *Destination, int Value, uint16_t Count);

__attribute__((noinline)) static void
intrinsic_memset(uint8_t *Destination, int Value, uint16_t Count) {
  __builtin_memset(Destination, Value, Count);
}

__attribute__((noinline)) static void
explicit_xdata_memset(__xdata uint8_t *Destination, int Value, uint16_t Count) {
  __builtin_memset(Destination, Value, Count);
}

__attribute__((noinline)) static void *
standard_memset(uint8_t *Destination, int Value, uint16_t Count) {
  return memset(Destination, Value, Count);
}

int main(void) {
  uint8_t Failures = 0;
  for (uint8_t ValueIndex = 0; ValueIndex != sizeof(Values) / sizeof(Values[0]);
       ++ValueIndex) {
    for (uint8_t LengthIndex = 0;
         LengthIndex != sizeof(Lengths) / sizeof(Lengths[0]); ++LengthIndex) {
      uint16_t Length = Lengths[LengthIndex];
      int Value = Values[ValueIndex];
      for (uint16_t I = 0; I != sizeof(Buffer); ++I)
        Buffer[I] = 0x5a;

      intrinsic_memset(&Buffer[3], Value, Length);
      uint8_t Byte = (uint8_t)Value;
      for (uint16_t I = 0; I != sizeof(Buffer); ++I) {
        uint8_t Expected = I >= 3 && I < 3 + Length ? Byte : 0x5a;
        if (Buffer[I] != Expected)
          Failures = 1;
      }
    }
  }

  for (uint8_t I = 0; I != sizeof(ExplicitXData); ++I)
    ExplicitXData[I] = 0x5a;
  explicit_xdata_memset(ExplicitXData + 1, 0x1234, 4);
  for (uint8_t I = 0; I != sizeof(ExplicitXData); ++I) {
    uint8_t Expected = I >= 1 && I < 5 ? 0x34 : 0x5a;
    if (ExplicitXData[I] != Expected)
      Failures = 1;
  }

  for (uint16_t I = 0; I != sizeof(Buffer); ++I)
    Buffer[I] = 0x5a;
  if (standard_memset(&Buffer[3], 0xa5, 16) != &Buffer[3])
    Failures = 1;
  for (uint16_t I = 0; I != sizeof(Buffer); ++I) {
    uint8_t Expected = I >= 3 && I < 19 ? 0xa5 : 0x5a;
    if (Buffer[I] != Expected)
      Failures = 1;
  }

  Result = Failures;
  for (;;) {
  }
}
