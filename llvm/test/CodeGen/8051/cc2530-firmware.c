// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=MAP
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS
// RUN: llvm-objcopy --output-target=ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

volatile unsigned char counter;

void entry(void) { counter = 1; }

// MAP: Name: .text
// MAP: Address: 0x0
// MAP: Name: .bss
// MAP: Address: 0x0
// MAP: Name: counter
// MAP: Value: 0x0

// DIS-LABEL: <reset>:
// DIS: ljmp
// DIS-LABEL: <__mcs51_start>:
// DIS: mov 129, #127
// DIS: mov dptr, #0
// DIS: movx @dptr, a
// DIS: lcall
// DIS-LABEL: <entry>:
// DIS: mov dptr, #0
// DIS: movx @dptr, a

// IHEX: :00000001FF
