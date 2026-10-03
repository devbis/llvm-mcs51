// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=MAP
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS
// RUN: llvm-objcopy --output-target=ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

typedef volatile unsigned char data8 __attribute__((address_space(1)));
typedef volatile unsigned char idata8 __attribute__((address_space(2)));

data8 direct_data = 1;
idata8 indirect_data = 2;
volatile unsigned char xdata_data = 3;

unsigned char read_direct_data(void) { return direct_data; }
unsigned char read_indirect_data(void) { return indirect_data; }

int main(void) {
  direct_data = 1;
  indirect_data = 2;
  xdata_data = 3;
  return 0;
}

// MAP: Name: .mcs51_data1
// MAP: Address: 0x48
// MAP: Name: .mcs51_data2
// MAP: Address: 0x49
// MAP: Name: direct_data
// MAP: Value: 0x48
// MAP: Name: indirect_data
// MAP: Value: 0x49

// DIS-LABEL: <__mcs51_start>:
// DIS: lcall
// DIS: lcall
// DIS: lcall
// DIS-LABEL: <main>:
// DIS: mov 72, #1
// DIS: mov r0, #73
// DIS: mov @r0, #2
// DIS: movx @dptr, a
// DIS-LABEL: <read_indirect_data>:
// DIS: mov r0, #73
// DIS: mov a, @r0
// DIS-LABEL: <read_direct_data>:
// DIS: mov a, 72

// IHEX: :01{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}0001
// IHEX: :01{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}0002
// IHEX: :00000001FF
