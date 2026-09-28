# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

        .text
        .globl entry
entry:
        mov     dptr, #xdata_symbol
        mov     a, #byte_symbol
        ljmp    code_symbol
        lcall   code_symbol
        sjmp    branch_symbol
        jb      0x20, branch_symbol
local_branch:
        sjmp    local_branch

        .data
        .2byte  code_symbol

        .globl byte_symbol
        .globl code_symbol
        .globl branch_symbol
        .globl xdata_symbol

# ENC: mov dptr, #xdata_symbol{{.*}}encoding: {{\[}}0x90,A,A{{\]}}
# ENC: mov a, #byte_symbol{{.*}}encoding: {{\[}}0x74,A{{\]}}
# ENC: ljmp code_symbol{{.*}}encoding: {{\[}}0x02,A,A{{\]}}
# ENC: lcall code_symbol{{.*}}encoding: {{\[}}0x12,A,A{{\]}}
# ENC: sjmp branch_symbol{{.*}}encoding: {{\[}}0x80,A{{\]}}
# ENC: jb 32, branch_symbol{{.*}}encoding: {{\[}}0x20,0x20,A{{\]}}
# ENC: sjmp local_branch{{.*}}encoding: {{\[}}0x80,A{{\]}}

# RELOC: R_8051_DPTR16 xdata_symbol
# RELOC: R_8051_8 byte_symbol
# RELOC: R_8051_16_BE code_symbol
# RELOC: R_8051_16_BE code_symbol
# RELOC: R_8051_PCREL8 branch_symbol
# RELOC: R_8051_PCREL8 branch_symbol
# RELOC: R_8051_16 code_symbol
