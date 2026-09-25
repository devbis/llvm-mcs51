# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

        nop
        inc a
        rl a
        mul ab
        movx a, @dptr
        mov a, #0x5a
        add a, #1
        mov a, r3
        add a, r3
        mov r3, a
        inc r3
        mov a, r0
        mov a, r7
        inc r7
        mov r4, #0x5a
        mov r4, r3
        ret
        reti

# ASM: nop{{.*}}encoding: [0x00]
# ASM: inc a{{.*}}encoding: [0x04]
# ASM: rl a{{.*}}encoding: [0x23]
# ASM: mul ab{{.*}}encoding: [0xa4]
# ASM: movx a, @dptr{{.*}}encoding: [0xe0]
# ASM: mov a, #90{{.*}}encoding: [0x74,0x5a]
# ASM: add a, #1{{.*}}encoding: [0x24,0x01]
# ASM: mov a, r3{{.*}}encoding: [0xeb]
# ASM: add a, r3{{.*}}encoding: [0x2b]
# ASM: mov r3, a{{.*}}encoding: [0xfb]
# ASM: inc r3{{.*}}encoding: [0x0b]
# ASM: mov a, r0{{.*}}encoding: [0xe8]
# ASM: mov a, r7{{.*}}encoding: [0xef]
# ASM: inc r7{{.*}}encoding: [0x0f]
# ASM: mov r4, #90{{.*}}encoding: [0x7c,0x5a]
# ASM: mov r4, r3{{.*}}encoding: [0x85,0x03,0x04]
# ASM: ret{{.*}}encoding: [0x22]
# ASM: reti{{.*}}encoding: [0x32]
# DIS: nop
# DIS: inc a
# DIS: rl a
# DIS: mul ab
# DIS: movx a, @dptr
# DIS: mov a, #90
# DIS: add a, #1
# DIS: mov a, r3
# DIS: add a, r3
# DIS: mov r3, a
# DIS: inc r3
# DIS: mov a, r0
# DIS: mov a, r7
# DIS: inc r7
# DIS: mov r4, #90
# DIS: mov r4, r3
# DIS: ret
# DIS: reti
