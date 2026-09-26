// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld -Wl,--no-check-sections %S/../../../lib/Target/MCS51/cc2530_startup.s %t.o -o %t.elf

__data volatile unsigned char data_value;
__idata volatile unsigned char idata_value;
__xdata volatile unsigned char xdata_value;
__code volatile const unsigned char code_value = 0x42;
__code volatile const unsigned short code_word = 0x1234;

unsigned char read_data(void) { return data_value; }
void write_data(unsigned char value) { data_value = value; }

unsigned char read_idata(void) { return idata_value; }
void write_idata(unsigned char value) { idata_value = value; }

unsigned char read_xdata(void) { return xdata_value; }
void write_xdata(unsigned char value) { xdata_value = value; }

unsigned char read_code(void) { return code_value; }
unsigned short read_code_word(void) { return code_word; }
unsigned char read_code_pointer(__code const unsigned char *pointer) {
  return *pointer;
}
unsigned char read_xdata_pointer(__xdata volatile unsigned char *pointer) {
  return *pointer;
}
void write_xdata_pointer(__xdata volatile unsigned char *pointer,
                         unsigned char value) {
  *pointer = value;
}

int main(void) {
  write_data(read_idata());
  write_idata(read_xdata());
  write_xdata(read_code());
  write_xdata(read_code_pointer(&code_value));
  write_xdata_pointer(&xdata_value, read_xdata_pointer(&xdata_value));
  return read_code_word();
}

// CHECK-LABEL: read_data:
// CHECK: mov a, data_value
// CHECK-LABEL: write_data:
// CHECK: mov data_value, a
// CHECK-LABEL: read_idata:
// CHECK: mov a, @r0
// CHECK-LABEL: write_idata:
// CHECK: mov @r0, a
// CHECK-LABEL: read_xdata:
// CHECK: mov dptr, #xdata_value
// CHECK: movx a, @dptr
// CHECK-LABEL: write_xdata:
// CHECK: mov dptr, #xdata_value
// CHECK: movx @dptr, a
// CHECK-LABEL: read_code:
// CHECK: mov dptr, #code_value
// CHECK: movc a, @a+dptr
// CHECK-LABEL: read_code_word:
// CHECK: mov dptr, #code_word
// CHECK: movc a, @a+dptr
// CHECK: inc dptr
// CHECK: movc a, @a+dptr
// CHECK-LABEL: read_code_pointer:
// CHECK: movc a, @a+dptr
// CHECK-LABEL: read_xdata_pointer:
// CHECK: movx a, @dptr
// CHECK-LABEL: write_xdata_pointer:
// CHECK: movx @dptr, a
