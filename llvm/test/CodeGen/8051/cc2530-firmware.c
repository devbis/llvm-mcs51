// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=MAP
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS
// RUN: llvm-objcopy --output-target=ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

volatile unsigned char counter;
unsigned char initialized = 42;

int main(void) {
  counter = initialized;
  return 0;
}

// MAP: Name: .text
// MAP: Address: 0x0
// MAP: Name: .data
// MAP: Address: 0x0
// MAP: Name: .bss
// MAP: Address: 0x1
// MAP: Name: initialized
// MAP: Value: 0x0
// MAP: Name: counter
// MAP: Value: 0x1
// MAP-DAG: Name: __cc2530_data_alias_start
// MAP-DAG: Value: 0x1F00
// MAP-DAG: Name: __cc2530_xreg_start
// MAP-DAG: Value: 0x6000
// MAP-DAG: Name: __cc2530_sfr_xdata_start
// MAP-DAG: Value: 0x7080
// MAP-DAG: Name: __cc2530_info_start
// MAP-DAG: Value: 0x7800
// MAP-DAG: Name: __cc2530_xbank_start
// MAP-DAG: Value: 0x8000

// DIS-LABEL: <reset>:
// DIS: ljmp
// DIS-LABEL: <__mcs51_start>:
// DIS: mov 129, #127
// DIS: mov dptr, #0
// DIS: movx @dptr, a
// DIS: lcall
// DIS: movc a, @a+dptr
// DIS-LABEL: <main>:
// DIS: mov dptr, #0
// DIS: movx @dptr, a

// IHEX: :01{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}002A
// IHEX: :00000001FF
