# Shrinking MOV DPTR,#b to INC DPTR must not retarget branches, jumps or code
# addresses that span it.  Objects without R_8051_RELAX markers, such as those
# from older assemblers or assembled without -mattr=+relax, are never shrunk.
# RUN: llvm-mc -triple=mcs51 -mattr=+relax -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 --section-start=.rodata=0x80 --section-start=.data=0x100 -e _start %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=RELAX
# RUN: llvm-objdump -s -j .rodata %t | FileCheck %s --check-prefix=TAB-RELAX
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 --section-start=.rodata=0x80 --section-start=.data=0x100 -e _start %t.o -o %t.norelax --no-relax
# RUN: llvm-objdump -d %t.norelax | FileCheck %s --check-prefix=NORELAX
# RUN: llvm-objdump -s -j .rodata %t.norelax | FileCheck %s --check-prefix=TAB-NORELAX

        .text
        .globl _start
_start:
        sjmp    .Lfwd
.Lback:
        mov     dptr, #a
        movx    @dptr, a
        mov     dptr, #b
        movx    @dptr, a
.Lfwd:
        jz      .Lback
        ljmp    .Lend
        nop
.Lend:
        ret

        .section .rodata,"a"
tab:    .short  .Lend

        .data
a:      .byte   0
b:      .byte   0

# RELOC:      .rela.text {
# RELOC-NEXT:   0x1 R_8051_PCREL8 .Lfwd 0xFFFFFFFF
# RELOC-NEXT:   0x3 R_8051_DPTR16 .data 0x0
# RELOC-NEXT:   0x3 R_8051_RELAX - 0x0
# RELOC-NEXT:   0x7 R_8051_DPTR16 .data 0x1
# RELOC-NEXT:   0x7 R_8051_RELAX - 0x0
# RELOC-NEXT:   0xB R_8051_PCREL8 .Lback 0xFFFFFFFF
# RELOC-NEXT:   0xD R_8051_16_BE .Lend 0x0
# RELOC-NEXT: }
# RELOC:      .rela.rodata {
# RELOC-NEXT:   0x0 R_8051_16 .Lend 0x0
# RELOC-NEXT: }

# RELAX:      0: 80 06 sjmp 8
# RELAX:      2: 90 01 00 mov dptr, #256
# RELAX:      6: a3 inc dptr
# RELAX:      8: 60 f8 jz 2
# RELAX-NEXT: a: 02 00 0e ljmp 14
# RELAX:      e: 22 ret
# TAB-RELAX:  0080 0e00

# NORELAX:      0: 80 08 sjmp 10
# NORELAX:      6: 90 01 01 mov dptr, #257
# NORELAX:      a: 60 f6 jz 2
# NORELAX-NEXT: c: 02 00 10 ljmp 16
# NORELAX:      10: 22 ret
# TAB-NORELAX:  0080 1000
