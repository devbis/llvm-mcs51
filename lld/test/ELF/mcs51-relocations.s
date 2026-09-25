# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 -e entry \
# RUN:   --defsym=byte_symbol=0x20 --defsym=code_symbol=0x1234 %t.o -o %t
# RUN: llvm-readobj --hex-dump=.text %t | FileCheck %s

        .text
        .globl entry
entry:
        mov     a, #byte_symbol
        ljmp    code_symbol
        lcall   code_symbol

        .globl byte_symbol
        .globl code_symbol

# CHECK: 0x00000000 74200234 12123412
