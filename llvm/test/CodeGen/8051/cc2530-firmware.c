// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections -Wl,-e,entry %s -o %t.elf
// RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=MAP
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

// IHEX: :00000001FF
