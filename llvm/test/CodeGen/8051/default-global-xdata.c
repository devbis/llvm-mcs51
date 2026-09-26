// RUN: clang -target mcs51 -mcpu=cc2530 -S -O1 %s -o - | FileCheck %s --check-prefix=ASM
// RUN: clang -target mcs51 -mcpu=cc2530 -S -emit-llvm -O0 %s -o - | FileCheck %s --check-prefix=IR

unsigned int global_counter;

unsigned int read_counter(void) { return global_counter; }

void write_counter(unsigned int value) { global_counter = value; }

// IR: @global_counter = {{.*}}addrspace(4) global i16 0

// ASM-LABEL: read_counter:
// ASM: mov dptr, #global_counter
// ASM: movx a, @dptr
// ASM: ret

// ASM-LABEL: write_counter:
// ASM: mov dptr, #global_counter
// ASM: movx @dptr, a
// ASM: inc dptr
// ASM: movx @dptr, a
// ASM: ret
