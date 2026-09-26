// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-readobj --sections %t.elf | FileCheck %s --check-prefix=MAP
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

volatile unsigned char ticks;

void __attribute__((interrupt(0))) timer0(void) { ++ticks; }
void __attribute__((interrupt(6))) dma(void) { ticks = 3; }
void __attribute__((interrupt(17))) rf(void) { ticks = 7; }
int main(void) { return ticks; }

// CHECK-LABEL: timer0:
// CHECK: push
// CHECK: reti
// CHECK: .section .mcs51.vector.0
// CHECK: ljmp timer0
// CHECK-LABEL: dma:
// CHECK: reti
// CHECK: .section .mcs51.vector.6
// CHECK: ljmp dma
// CHECK-LABEL: rf:
// CHECK: reti
// CHECK: .section .mcs51.vector.17
// CHECK: ljmp rf

// MAP-DAG: Name: .vectors
// MAP-DAG: Address: 0x0
// MAP-DAG: Name: .mcs51_vector_0
// MAP-DAG: Address: 0x3
// MAP-DAG: Name: .mcs51_vector_6
// MAP-DAG: Address: 0x33
// MAP-DAG: Name: .mcs51_vector_17
// MAP-DAG: Address: 0x8B
// MAP-DAG: Name: .text
// MAP-DAG: Address: 0x94
// DIS: 00000000 <reset>:
// DIS: ljmp
// DIS: 00000003 <.mcs51_vector_0>:
// DIS: ljmp
// DIS: 00000033 <.mcs51_vector_6>:
// DIS: ljmp
// DIS: 0000008b <.mcs51_vector_17>:
// DIS: ljmp
