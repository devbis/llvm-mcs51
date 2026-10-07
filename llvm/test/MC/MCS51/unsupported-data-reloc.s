# MCS-51 has no 32-bit data relocation; the assembler must diagnose it instead
# of emitting an unrelated relocation.
# RUN: not llvm-mc -triple=mcs51 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

        .text
start:
        nop
end:
        .data
# CHECK: :[[#@LINE+1]]:{{[0-9]+}}: error: unsupported MCS-51 relocation
        .long   start
