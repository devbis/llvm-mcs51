# Relocated 8-bit direct/immediate operands must be fixed up at the byte that
# holds that operand.  For MOV direct,direct the destination is the third
# byte; for <op> direct,#imm the immediate is the third byte.
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s

        .text
f:
        mov     dv+2, 48
        mov     48, dv+1
        mov     dv, dw
        mov     dv, #dw
        anl     dv, #dw
        orl     dv+1, #3
        xrl     dv, #dw
        mov     dv, a

        .data
dv:     .byte   0, 0, 0, 0
dw:     .byte   0

# CHECK:      Section {{.*}} .rela.text {
# CHECK-NEXT:   0x2 R_8051_8 .data 0x2
# CHECK-NEXT:   0x4 R_8051_8 .data 0x1
# CHECK-NEXT:   0x8 R_8051_8 .data 0x0
# CHECK-NEXT:   0x7 R_8051_8 .data 0x4
# CHECK-NEXT:   0xA R_8051_8 .data 0x0
# CHECK-NEXT:   0xB R_8051_8 .data 0x4
# CHECK-NEXT:   0xD R_8051_8 .data 0x0
# CHECK-NEXT:   0xE R_8051_8 .data 0x4
# CHECK-NEXT:   0x10 R_8051_8 .data 0x1
# CHECK-NEXT:   0x13 R_8051_8 .data 0x0
# CHECK-NEXT:   0x14 R_8051_8 .data 0x4
# CHECK-NEXT:   0x16 R_8051_8 .data 0x0
# CHECK-NEXT: }
