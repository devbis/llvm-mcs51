# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0x100 -e entry %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s

        .text
        .globl entry
        .type entry,@function
entry:
        ljmp .Lnear
.space 8
.Lnear:
        ret
        ljmp .Lfar
        .space 2050
.Lfar:
        ret
        .size entry, .-entry

# CHECK: 100: {{.*}} ajmp 266
# CHECK: 10b: {{.*}} ljmp 2320
