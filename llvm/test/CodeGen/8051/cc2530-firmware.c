// RUN: clang -target mcs51 -mcpu=cc2530 -O2 %s -o %t.elf
// RUN: llvm-objcopy --output-target=ihex %t.elf %t.hex
// RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=MAP \
// RUN:   --implicit-check-not=unused_initialized \
// RUN:   --implicit-check-not=unused_firmware_function
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

volatile unsigned char counter;
unsigned char initialized = 42;
unsigned char unused_initialized = 77;

int unused_firmware_function(int value) { return value * 13 + 7; }

int main(void) {
  counter = initialized;
  return 0;
}

// MAP: Name: .text
// MAP: Address: 0x94
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
// DIS-NEXT: mov r6, #0
// DIS-NEXT: mov r7, #32
// DIS-NEXT: clr a
// DIS-NEXT: movx @dptr, a
// DIS-NEXT: inc dptr
// DIS-NEXT: inc r6
// DIS-NEXT: cjne r6, #0,
// DIS-NEXT: dec r7
// DIS-NEXT: mov a, r6
// DIS-NEXT: orl a, r7
// DIS-NEXT: jnz
// DIS: lcall
// DIS: movc a, @a+dptr
// DIS-LABEL: <main>:
// DIS: mov dptr, #0
// DIS: movx @dptr, a

// IHEX: :01{{[0-9A-F][0-9A-F][0-9A-F][0-9A-F]}}002A
// IHEX: :00000001FF
