// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 %s -o %t.elf
// RUN: clang -target mcs51 -mcpu=cc2530 -fno-function-sections \
// RUN:   -fno-data-sections -O2 %s -o %t.flat.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

typedef __generic volatile unsigned char *generic_byte_ptr;
typedef __generic volatile unsigned short *generic_word_ptr;
typedef __generic volatile unsigned long *generic_long_ptr;
typedef __generic volatile unsigned long long *generic_quad_ptr;
typedef __generic volatile float *generic_float_ptr;
typedef __generic const volatile unsigned char *generic_const_byte_ptr;

typedef struct {
  unsigned char Byte;
  unsigned short Word;
} GenericAggregate;
typedef __generic GenericAggregate *generic_aggregate_ptr;

__code volatile const unsigned char code_byte = 0x81;
__xdata volatile const unsigned char xdata_byte = 0x42;
__pdata volatile const unsigned char pdata_byte = 0x24;
__idata volatile const unsigned char idata_byte = 0x18;
__xdata volatile unsigned char xdata_target;
__pdata volatile unsigned char pdata_target;
__idata volatile unsigned char idata_target;

unsigned char load_generic_byte(generic_byte_ptr Pointer) { return *Pointer; }

void store_generic_byte(generic_byte_ptr Pointer, unsigned char Value) {
  *Pointer = Value;
}

__attribute__((noinline)) GenericAggregate
load_generic_aggregate(generic_aggregate_ptr Pointer) {
  return *Pointer;
}

__attribute__((noinline)) void
store_generic_aggregate(generic_aggregate_ptr Pointer, unsigned char Byte,
                        unsigned short Word) {
  GenericAggregate Value = {Byte, Word};
  *Pointer = Value;
}

unsigned short load_generic_word(generic_word_ptr Pointer) { return *Pointer; }

void store_generic_word(generic_word_ptr Pointer, unsigned short Value) {
  *Pointer = Value;
}

unsigned long load_generic_long(generic_long_ptr Pointer) { return *Pointer; }

void store_generic_long(generic_long_ptr Pointer, unsigned long Value) {
  *Pointer = Value;
}

unsigned long long load_generic_quad(generic_quad_ptr Pointer) {
  return *Pointer;
}

void store_generic_quad(generic_quad_ptr Pointer, unsigned long long Value) {
  *Pointer = Value;
}

float load_generic_float(generic_float_ptr Pointer) { return *Pointer; }

unsigned char read_generic_code(void) {
  return *(generic_const_byte_ptr)&code_byte;
}

unsigned char read_generic_xdata(void) {
  return *(generic_const_byte_ptr)&xdata_byte;
}

unsigned char read_generic_pdata(void) {
  return *(generic_const_byte_ptr)&pdata_byte;
}

unsigned char read_generic_idata(__idata const volatile unsigned char *Pointer) {
  return *(generic_const_byte_ptr)Pointer;
}

unsigned char read_generic_idata_global(void) {
  return *(generic_const_byte_ptr)&idata_byte;
}

unsigned char read_via_idata_cast(generic_const_byte_ptr Pointer) {
  return *(__idata const volatile unsigned char *)Pointer;
}

void write_generic_xdata(void) {
  *(generic_byte_ptr)&xdata_target = 0x51;
}

void write_generic_pdata(void) {
  *(generic_byte_ptr)&pdata_target = 0x52;
}

void write_generic_idata(void) {
  *(generic_byte_ptr)&idata_target = 0x53;
}

void write_generic_code(void) {
  *(generic_byte_ptr)&code_byte = 0x54;
}

generic_const_byte_ptr generic_code_address(void) {
  return (generic_const_byte_ptr)&code_byte;
}

generic_const_byte_ptr generic_xdata_address(void) {
  return (generic_const_byte_ptr)&xdata_byte;
}

generic_const_byte_ptr generic_pdata_address(void) {
  return (generic_const_byte_ptr)&pdata_byte;
}

generic_const_byte_ptr generic_idata_address(void) {
  return (generic_const_byte_ptr)&idata_byte;
}

__attribute__((noinline)) unsigned char load_via_generic(
    generic_const_byte_ptr Pointer) {
  return *Pointer;
}

unsigned char read_generic_stack(void) {
  volatile unsigned char Local = 0x5a;
  return load_via_generic((generic_const_byte_ptr)&Local);
}

void store_generic_float(generic_float_ptr Pointer, float Value) {
  *Pointer = Value;
}

int main(void) {
  generic_byte_ptr Pointer = (generic_byte_ptr)0;
  store_generic_byte(Pointer, 0x5a);
  generic_word_ptr Word = (generic_word_ptr)0;
  store_generic_word(Word, 0x1234);
  generic_long_ptr Long = (generic_long_ptr)0;
  store_generic_long(Long, 0x12345678UL);
  generic_quad_ptr Quad = (generic_quad_ptr)0;
  store_generic_quad(Quad, 0x123456789abcdef0ULL);
  generic_float_ptr Float = (generic_float_ptr)0;
  store_generic_float(Float, 1.5f);
  return load_generic_byte(Pointer) != 0x5a ||
         load_generic_word(Word) != 0x1234 ||
         load_generic_long(Long) != 0x12345678UL ||
         load_generic_quad(Quad) != 0x123456789abcdef0ULL ||
         load_generic_float(Float) != 1.5f;
}

// ASM-LABEL: load_generic_byte:
// ASM: lcall __mcs51_gptrget8
// ASM-LABEL: store_generic_byte:
// ASM: lcall __mcs51_gptrput8
// ASM-LABEL: load_generic_aggregate:
// ASM: lcall __mcs51_gptrget32
// ASM-LABEL: store_generic_aggregate:
// ASM: lcall __mcs51_gptrput8
// ASM: lcall __mcs51_gptrput8
// ASM: lcall __mcs51_gptrput16
// ASM-LABEL: load_generic_word:
// ASM: lcall __mcs51_gptrget16
// ASM-LABEL: store_generic_word:
// ASM: lcall __mcs51_gptrput16
// ASM-LABEL: load_generic_long:
// ASM: lcall __mcs51_gptrget32
// ASM-LABEL: store_generic_long:
// ASM: lcall __mcs51_gptrput32
// ASM-LABEL: load_generic_quad:
// ASM: lcall __mcs51_gptrget64
// ASM-LABEL: store_generic_quad:
// ASM: lcall __mcs51_gptrput64
// ASM-LABEL: load_generic_float:
// ASM: lcall __mcs51_gptrgetf32
// ASM-LABEL: read_generic_code:
// ASM: movc a, @a+dptr
// ASM-LABEL: read_generic_xdata:
// ASM: movx a, @dptr
// ASM-LABEL: read_generic_pdata:
// ASM: movx a, @r
// ASM-LABEL: read_generic_idata:
// ASM: mov a, @r
// ASM-LABEL: read_generic_idata_global:
// ASM: mov a, @r
// ASM-LABEL: read_via_idata_cast:
// ASM: mov a, @r
// ASM-LABEL: write_generic_xdata:
// ASM: movx @dptr, a
// ASM-LABEL: write_generic_pdata:
// ASM: movx @r0, a
// ASM-LABEL: write_generic_idata:
// ASM: mov @r0, #83
// ASM-LABEL: write_generic_code:
// ASM: lcall __mcs51_gptrput8
// ASM-LABEL: generic_code_address:
// ASM: mov r6, #-128
// ASM-LABEL: generic_pdata_address:
// ASM: mov r6, #96
// ASM-LABEL: generic_idata_address:
// ASM: mov r6, #64
// ASM-LABEL: read_generic_stack:
// ASM: mov r{{[0-7]}}, #64
// ASM-LABEL: store_generic_float:
// ASM: lcall __mcs51_gptrputf32
// LINK: Name: __mcs51_gptrget8
// LINK: Name: __mcs51_gptrput8
// DIS: lcall
