// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-Ttext=0 -Wl,-e,entry %s -o %t.elf
// RUN: llvm-objcopy --output-target=ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

void entry(void) {}

// IHEX: :0100000022DD
// IHEX-NEXT: :00000001FF
