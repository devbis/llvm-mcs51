// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -ffreestanding -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld -Wl,--no-check-sections %S/../../../lib/Target/MCS51/cc2530_startup.s %t.o -o %t.elf
// RUN: llvm-readobj --sections --symbols %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=START
// RUN: llvm-objcopy -O ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

__bit volatile unsigned char zero_bit;
__bit volatile unsigned char initialized_one = 3;
__bit volatile unsigned char initialized_zero = 0;

unsigned char read_zero_bit(void) { return zero_bit; }
unsigned char read_initialized_one(void) { return initialized_one; }
void write_zero_bit(unsigned char value) { zero_bit = value; }
void set_zero_bit(void) { zero_bit = 1; }
void clear_zero_bit(void) { zero_bit = 0; }

int main(void) {
  write_zero_bit(read_initialized_one());
  set_zero_bit();
  clear_zero_bit();
  return 0;
}

// ASM-LABEL: read_zero_bit:
// ASM: mov c, zero_bit
// ASM: clr a
// ASM: rlc a
// ASM-LABEL: read_initialized_one:
// ASM: mov c, initialized_one
// ASM: clr a
// ASM: rlc a
// ASM-LABEL: write_zero_bit:
// ASM: mov c, 224
// ASM: mov zero_bit, c
// ASM-LABEL: set_zero_bit:
// ASM: setb zero_bit
// ASM-LABEL: clear_zero_bit:
// ASM: clr zero_bit

// LINK: Name: .mcs51_bit
// LINK: Address: 0x0
// LINK: Size: 1
// LINK: Name: .mcs51_bit_bss
// LINK: Address: 0x1
// LINK: Size: 2
// LINK: Name: zero_bit
// LINK: Value: 0x2
// LINK: Section: .mcs51_bit_bss
// LINK: Name: initialized_one
// LINK: Value: 0x0
// LINK: Section: .mcs51_bit
// LINK: Name: initialized_zero
// LINK: Value: 0x1
// LINK: Section: .mcs51_bit_bss
// IHEX: :00000001FF

// The regular data copier preserves all eight source bits. The bit initializer
// reads one byte per object and stores only its low bit in the bit-address area.
// START: movc a, @a+dptr
// START-NEXT: mov r6, a
// START: movc a, @a+dptr
// START-NEXT: anl a, #1
// START-NEXT: mov r6, a
