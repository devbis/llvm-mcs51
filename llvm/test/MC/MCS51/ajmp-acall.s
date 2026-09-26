# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

        .text
        ajmp    0x1234
        acall   0x1234
        ajmp    target
        acall   target
        .globl  target
target:
        nop

# ENC: ajmp 4660{{.*}}encoding: {{\[}}0x41,0x34{{\]}}
# ENC: acall 4660{{.*}}encoding: {{\[}}0x51,0x34{{\]}}
# ENC: ajmp target ; encoding: [0x01'A',0b00000AAA]
# ENC: acall target ; encoding: [0x11'A',0b00000AAA]

# RELOC: R_8051_11 target
# RELOC: R_8051_11 target
