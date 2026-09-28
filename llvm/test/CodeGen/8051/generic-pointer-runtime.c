// Runtime fixture for verify-generic-pointer-runtime.sh.
// Keep the result in XDATA so uCsim can inspect it after the first write.
typedef __generic volatile unsigned char *generic_byte_ptr;
typedef __generic volatile unsigned long long *generic_quad_ptr;

volatile __xdata unsigned char Result;

int main(void) {
  volatile unsigned char Byte = 0;
  volatile unsigned long long Quad = 0;
  generic_byte_ptr BytePointer = (generic_byte_ptr)&Byte;
  generic_quad_ptr QuadPointer = (generic_quad_ptr)&Quad;

  *BytePointer = 0x5a;
  *QuadPointer = 0x123456789abcdef0ULL;
  Result = *BytePointer != 0x5a || *QuadPointer != 0x123456789abcdef0ULL;
  for (;;) {}
}
