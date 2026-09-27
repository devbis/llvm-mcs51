# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 --defsym=target=0x1234 %t.o -o %t
# RUN: llvm-readobj --hex-dump=.data %t | FileCheck %s

        .data
        .2byte target

# CHECK: 0x00000000 3412
