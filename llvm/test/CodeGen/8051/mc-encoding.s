# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

        nop
        inc a
        rl a
        mul ab
        movx a, @dptr
        mov a, #0x5a
        mov dptr, #0x1234
        add a, #1
        subb a, #1
        mov a, r3
        mov b, r3
        add a, r3
        subb a, r3
        mov r3, a
        inc r3
        mov a, r0
        mov a, r7
        inc r7
        mov r4, #0x5a
        mov r4, r3
        mov a, 0x80
        mov 0x90, a
        mov 0x90, #0x5a
        inc 0x90
        push 0xe0
        pop 0xe0
        mov a, @r0
        mov a, @r1
        mov @r0, a
        mov @r1, a
        mov @r0, #0x5a
        mov @r1, #0x5a
        inc @r0
        inc @r1
        mov c, 0x20
        mov 0x20, c
        clr 0x20
        setb 0x20
        cpl 0x20
        anl c, 0x20
        anl c, /0x20
        orl c, 0x20
        orl c, /0x20
        ret
        reti

# ASM: nop{{.*}}encoding: [0x00]
# ASM: inc a{{.*}}encoding: [0x04]
# ASM: rl a{{.*}}encoding: [0x23]
# ASM: mul ab{{.*}}encoding: [0xa4]
# ASM: movx a, @dptr{{.*}}encoding: [0xe0]
# ASM: mov a, #90{{.*}}encoding: [0x74,0x5a]
# ASM: mov dptr, #4660{{.*}}encoding: [0x90,0x34,0x12]
# ASM: add a, #1{{.*}}encoding: [0x24,0x01]
# ASM: subb a, #1{{.*}}encoding: [0x94,0x01]
# ASM: mov a, r3{{.*}}encoding: [0xeb]
# ASM: mov b, r3{{.*}}encoding: [0x8b]
# ASM: add a, r3{{.*}}encoding: [0x2b]
# ASM: subb a, r3{{.*}}encoding: [0x9b]
# ASM: mov r3, a{{.*}}encoding: [0xfb]
# ASM: inc r3{{.*}}encoding: [0x0b]
# ASM: mov a, r0{{.*}}encoding: [0xe8]
# ASM: mov a, r7{{.*}}encoding: [0xef]
# ASM: inc r7{{.*}}encoding: [0x0f]
# ASM: mov r4, #90{{.*}}encoding: [0x7c,0x5a]
# ASM: mov r4, r3{{.*}}encoding: [0x85,0x03,0x04]
# ASM: mov a, 128{{.*}}encoding: [0xe5,0x80]
# ASM: mov 144, a{{.*}}encoding: [0xf5,0x90]
# ASM: mov 144, #90{{.*}}encoding: [0x75,0x90,0x5a]
# ASM: inc 144{{.*}}encoding: [0x05,0x90]
# ASM: push 224{{.*}}encoding: [0xc0,0xe0]
# ASM: pop 224{{.*}}encoding: [0xd0,0xe0]
# ASM: mov a, @r0{{.*}}encoding: [0xe6]
# ASM: mov a, @r1{{.*}}encoding: [0xe7]
# ASM: mov @r0, a{{.*}}encoding: [0xf6]
# ASM: mov @r1, a{{.*}}encoding: [0xf7]
# ASM: mov @r0, #90{{.*}}encoding: [0x76,0x5a]
# ASM: mov @r1, #90{{.*}}encoding: [0x77,0x5a]
# ASM: inc @r0{{.*}}encoding: [0x06]
# ASM: inc @r1{{.*}}encoding: [0x07]
# ASM: mov c, 32{{.*}}encoding: [0xa2,0x20]
# ASM: mov 32, c{{.*}}encoding: [0x92,0x20]
# ASM: clr 32{{.*}}encoding: [0xc2,0x20]
# ASM: setb 32{{.*}}encoding: [0xd2,0x20]
# ASM: cpl 32{{.*}}encoding: [0xb2,0x20]
# ASM: anl c, 32{{.*}}encoding: [0x82,0x20]
# ASM: anl c, /32{{.*}}encoding: [0xb0,0x20]
# ASM: orl c, 32{{.*}}encoding: [0x72,0x20]
# ASM: orl c, /32{{.*}}encoding: [0xa0,0x20]
# ASM: ret{{.*}}encoding: [0x22]
# ASM: reti{{.*}}encoding: [0x32]
# DIS: nop
# DIS: inc a
# DIS: rl a
# DIS: mul ab
# DIS: movx a, @dptr
# DIS: mov a, #90
# DIS: mov dptr, #4660
# DIS: add a, #1
# DIS: subb a, #1
# DIS: mov a, r3
# DIS: mov b, r3
# DIS: add a, r3
# DIS: subb a, r3
# DIS: mov r3, a
# DIS: inc r3
# DIS: mov a, r0
# DIS: mov a, r7
# DIS: inc r7
# DIS: mov r4, #90
# DIS: mov r4, r3
# DIS: mov a, 128
# DIS: mov 144, a
# DIS: mov 144, #90
# DIS: inc 144
# DIS: push 224
# DIS: pop 224
# DIS: mov a, @r0
# DIS: mov a, @r1
# DIS: mov @r0, a
# DIS: mov @r1, a
# DIS: mov @r0, #90
# DIS: mov @r1, #90
# DIS: inc @r0
# DIS: inc @r1
# DIS: mov c, 32
# DIS: mov 32, c
# DIS: clr 32
# DIS: setb 32
# DIS: cpl 32
# DIS: anl c, 32
# DIS: anl c, /32
# DIS: orl c, 32
# DIS: orl c, /32
# DIS: ret
# DIS: reti
