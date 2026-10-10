# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0x7fc --section-start=.target=0x800 -e entry %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s

# A shortened instruction at 0x7fd has next PC 0x7ff. The target at 0x800
# is outside its 2 KiB page, even though the original three-byte instruction's
# next PC would be 0x800. Keep the long transfer so it remains valid.

        .section .text,"ax"
        .space 1
        .globl entry
entry:
        ljmp target

        .section .target,"ax"
        .globl target
target:
        ret

# CHECK: 7fd: 02 08 00 ljmp 2048
