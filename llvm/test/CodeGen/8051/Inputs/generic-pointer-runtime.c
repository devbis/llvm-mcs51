// Runtime fixture for verify-generic-pointer-runtime.sh.
// Keep the result in a dedicated XDATA slot so uCsim can inspect it after the
// test has completed writes through all other address spaces.
typedef __generic volatile unsigned char *generic_byte_ptr;
typedef __generic volatile unsigned long long *generic_quad_ptr;

__code const volatile unsigned char CodeValue = 0xa5;
__xdata volatile unsigned char XDataValue;
__pdata volatile unsigned char PDataValue;
__data volatile unsigned char DataValue;
__idata volatile unsigned char IDataValue;
__attribute__((section(".mcs51.simresult")))
volatile __xdata unsigned char Result;

int main(void) {
  volatile unsigned char Byte = 0;
  volatile unsigned long long Quad = 0;
  generic_byte_ptr BytePointer = (generic_byte_ptr)&Byte;
  generic_quad_ptr QuadPointer = (generic_quad_ptr)&Quad;
  generic_byte_ptr CodePointer = (generic_byte_ptr)&CodeValue;
  generic_byte_ptr XDataPointer = (generic_byte_ptr)&XDataValue;
  generic_byte_ptr PDataPointer = (generic_byte_ptr)&PDataValue;
  generic_byte_ptr DataPointer = (generic_byte_ptr)&DataValue;
  generic_byte_ptr IDataPointer = (generic_byte_ptr)&IDataValue;

  *BytePointer = 0x5a;
  *QuadPointer = 0x123456789abcdef0ULL;
  *XDataPointer = 0x11;
  *PDataPointer = 0x22;
  *DataPointer = 0x2a;
  *IDataPointer = 0x33;
  Result = (*CodePointer != 0xa5) |
           ((*BytePointer != 0x5a) << 1) |
           ((*QuadPointer != 0x123456789abcdef0ULL) << 2) |
           ((*XDataPointer != 0x11) << 3) | ((*PDataPointer != 0x22) << 4) |
           ((*DataPointer != 0x2a) << 5) | ((*IDataPointer != 0x33) << 6);
  for (;;) {}
}
