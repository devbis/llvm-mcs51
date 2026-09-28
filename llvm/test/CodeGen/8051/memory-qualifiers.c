// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld -Wl,--no-check-sections %S/../../../lib/Target/MCS51/cc2530_startup.s %t.o -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK

#define SFR(address) (*(volatile __sfr unsigned char *)(address))
#define SBIT(address) (*(volatile __sbit unsigned char *)(address))

__data volatile unsigned char data_value;
__idata volatile unsigned char idata_value;
__pdata volatile unsigned char pdata_value = 0x35;
__pdata volatile unsigned short pdata_word = 0x1234;
__pdata volatile unsigned char pdata_zero;
__pdata volatile unsigned char pdata_array[4];
__xdata volatile unsigned char xdata_value;
__code volatile const unsigned char code_value = 0x42;
__code volatile const unsigned short code_word = 0x1234;

unsigned char read_data(void) { return data_value; }
void write_data(unsigned char value) { data_value = value; }
void write_data_constant(void) { data_value = 0x42; }

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
unsigned char read_pdata_pointer(__pdata volatile unsigned char *pointer) {
  return *pointer;
}
__attribute__((noinline)) unsigned char read_pdata_indexed(unsigned char index) {
  return pdata_array[index];
}
__attribute__((noinline)) unsigned char read_pdata_global(void) {
  return pdata_value;
}
__attribute__((noinline)) void write_pdata_global(unsigned char value) {
  pdata_zero = value;
}
__attribute__((noinline)) unsigned short read_pdata_word_global(void) {
  return pdata_word;
}
__attribute__((noinline)) void write_pdata_word_global(unsigned short value) {
  pdata_word = value;
}
void write_pdata_pointer(__pdata volatile unsigned char *pointer,
                         unsigned char value) {
  *pointer = value;
}
unsigned char read_port0(void) { return SFR(0x80); }
void write_port0(unsigned char value) { SFR(0x80) = value; }
void write_port0_constant(void) { SFR(0x80) = 0x42; }
unsigned char read_ea(void) { return SBIT(0xaf); }
void set_ea(void) { SBIT(0xaf) = 1; }

int main(void) {
  write_data(read_idata());
  write_idata(read_xdata());
  write_xdata(read_code());
  write_xdata(read_code_pointer(&code_value));
  write_xdata_pointer(&xdata_value, read_xdata_pointer(&xdata_value));
  write_pdata_pointer(&pdata_zero, read_pdata_pointer(&pdata_value));
  write_port0(read_port0());
  set_ea();
  return read_code_word();
}

// CHECK-LABEL: read_data:
// CHECK: mov a, data_value
// CHECK-LABEL: write_data:
// CHECK: mov data_value, a
// CHECK-LABEL: write_data_constant:
// CHECK: mov data_value, #66
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
// CHECK-LABEL: read_pdata_pointer:
// CHECK: movx a, @r
// CHECK-LABEL: read_pdata_indexed:
// CHECK: movx a, @r
// CHECK-LABEL: read_pdata_global:
// CHECK: mov r0, #pdata_value
// CHECK: movx a, @r0
// CHECK-LABEL: write_pdata_global:
// CHECK: mov r0, #pdata_zero
// CHECK: movx @r0, a
// CHECK-LABEL: read_pdata_word_global:
// CHECK: movx a, @r
// CHECK: movx a, @r
// CHECK-LABEL: write_pdata_word_global:
// CHECK: movx @r0, a
// CHECK: movx @r0, a
// CHECK-LABEL: write_pdata_pointer:
// CHECK: movx @r0, a
// CHECK-LABEL: read_port0:
// CHECK: mov a, -128
// CHECK-LABEL: write_port0:
// CHECK: mov -128, a
// CHECK-LABEL: write_port0_constant:
// CHECK: mov -128, #66
// CHECK-LABEL: read_ea:
// CHECK: mov c, -81
// CHECK-LABEL: set_ea:
// CHECK: setb -81

// LINK: Name: pdata_value
// LINK-NEXT: Value: 0x0
// LINK: Name: pdata_zero
// LINK-NEXT: Value: 0x8
// LINK: Name: pdata_word
// LINK-NEXT: Value: 0x2
