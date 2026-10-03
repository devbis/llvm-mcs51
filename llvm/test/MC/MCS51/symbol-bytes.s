# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

# Bytes of a symbol's address are loaded into the imaginary registers
# (direct addresses 48-71) with lo8/hi8 and resolved by the linker.
        .text
        mov     48, #lo8(sym+14)
        mov     49, #hi8(sym+14)
        mov     71, #lo8(other)

# RELOC:      R_8051_LO8 sym 0xE
# RELOC-NEXT: R_8051_HI8 sym 0xE
# RELOC-NEXT: R_8051_LO8 other 0x0
# DIS: 75 30 00 mov 48, #0
# DIS: 75 31 00 mov 49, #0
# DIS: 75 47 00 mov 71, #0
