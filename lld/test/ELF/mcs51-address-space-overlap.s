# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -T %S/mcs51-address-space-overlap.ld \
# RUN:   --no-check-sections %t.o -o %t
# RUN: llvm-readobj --sections --symbols %t | FileCheck %s --check-prefix=OVERLAP

        .section .text.reset,"ax",@progbits
        .globl reset
reset:
        mov dptr, #xdata16(xdata_table)
        movx a, @dptr
        mov dptr, #code16(code_table)
        clr a
        movc a, @a+dptr
        sjmp reset

        .section .mcs51.xdata.init.xdata_table,"aw",@progbits
        .globl xdata_table
xdata_table:
        .byte 1

        .section .mcs51.code.rodata.code_table,"a",@progbits
        .globl code_table
code_table:
        .byte 2

# OVERLAP-DAG: Name: .data
# OVERLAP-DAG: Address: 0x100
# OVERLAP-DAG: Name: .code_table
# OVERLAP-DAG: Address: 0x100
# OVERLAP-DAG: Name: code_table
# OVERLAP-DAG: Value: 0x100
# OVERLAP-DAG: Name: xdata_table
# OVERLAP-DAG: Value: 0x100
