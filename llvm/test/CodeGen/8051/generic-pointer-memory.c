// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

typedef __generic volatile unsigned char *generic_byte_ptr;
typedef __generic volatile unsigned short *generic_word_ptr;
typedef __generic volatile unsigned long *generic_long_ptr;
typedef __generic volatile unsigned long long *generic_quad_ptr;
typedef __generic volatile float *generic_float_ptr;

unsigned char load_generic_byte(generic_byte_ptr Pointer) { return *Pointer; }

void store_generic_byte(generic_byte_ptr Pointer, unsigned char Value) {
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
// ASM-LABEL: store_generic_float:
// ASM: lcall __mcs51_gptrputf32
// LINK: Name: __mcs51_gptrget8
// LINK: Name: __mcs51_gptrput8
// DIS: lcall
