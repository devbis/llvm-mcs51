# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

        .text
        .globl entry
entry:
.Lloop:
        jz      .Ldone
        lcall   .Lhelper
        sjmp    .Lloop
        ljmp    .Ldone
.Lhelper:
        ret
.Ldone:
        ret

# Both relative branches cross at least one direct transfer that LLD may
# shorten. Keep them relocatable so the linker can recompute their offsets.
# RELOC: R_8051_PCREL8 .Ldone
# RELOC: R_8051_16_BE .Lhelper
# RELOC: R_8051_RELAX
# RELOC: R_8051_PCREL8 .Lloop
# RELOC: R_8051_16_BE .Ldone
# RELOC: R_8051_RELAX
