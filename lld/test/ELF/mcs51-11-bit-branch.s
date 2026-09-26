# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0x7f8 -e entry --defsym=target=0x7fa %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RUN: not ld.lld -m elf32-mcs51 -Ttext=0x7fc -e entry --defsym=target=0x800 %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=ERR

        .text
        .globl entry
entry:
        ajmp target
        acall target

# DIS: 7f8: e1 fa ajmp 2042
# DIS: 7fa: f1 fa acall 2042

# ERR: MCS-51 AJMP/ACALL target is outside the current 2 KiB page
