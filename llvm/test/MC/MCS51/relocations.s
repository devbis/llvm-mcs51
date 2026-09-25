# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

        .text
        .globl entry
entry:
        mov     a, #byte_symbol
        ljmp    code_symbol
        lcall   code_symbol

        .globl byte_symbol
        .globl code_symbol

# ENC: mov a, #byte_symbol{{.*}}encoding: {{\[}}0x74,A{{\]}}
# ENC: ljmp code_symbol{{.*}}encoding: {{\[}}0x02,A,A{{\]}}
# ENC: lcall code_symbol{{.*}}encoding: {{\[}}0x12,A,A{{\]}}

# RELOC: R_8051_8 byte_symbol
# RELOC: R_8051_16 code_symbol
# RELOC: R_8051_16 code_symbol
