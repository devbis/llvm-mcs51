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
        mov 0xf0, r3
        add a, r3
        subb a, r3
        mov r3, a
        inc r3
        mov a, r0
        mov a, r7
        inc r7
        dec r3
        mov r4, #0x5a
        mov 0x20, 0x21
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
        dec @r0
        dec @r1
        mov c, 0x20
        mov 0x20, c
        clr 0x20
        setb 0x20
        cpl 0x20
        anl c, 0x20
        anl c, /0x20
        orl c, 0x20
        orl c, /0x20
        xch a, r3
        xch a, 0x20
        xch a, @r0
        xchd a, @r1
        cjne a, #0x12, .Lcjne_a_imm
.Lcjne_a_imm:
        cjne a, 0x20, .Lcjne_a_direct
.Lcjne_a_direct:
        cjne @r0, #0x12, .Lcjne_ind
.Lcjne_ind:
        cjne r3, #0x12, .Lcjne_rn
.Lcjne_rn:
        djnz 0x20, .Ldjnz_direct
.Ldjnz_direct:
        djnz r3, .Ldjnz_rn
.Ldjnz_rn:
        add a, 0x20
        addc a, 0x20
        subb a, 0x20
        anl a, 0x20
        orl a, 0x20
        xrl a, 0x20
        add a, @r0
        addc a, @r1
        subb a, @r0
        anl a, @r1
        orl a, @r0
        xrl a, @r1
        anl 0x20, #0x12
        orl 0x20, #0x12
        xrl 0x20, #0x12
        mov r3, 0x20
        mov 0x20, r3
        mov @r0, 0x20
        mov @r1, 0x20
        mov 0x20, @r0
        mov 0x20, @r1
        anl 0x20, a
        orl 0x20, a
        xrl 0x20, a
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
# ASM: mov 240, r3{{.*}}encoding: [0x8b,0xf0]
# ASM: add a, r3{{.*}}encoding: [0x2b]
# ASM: subb a, r3{{.*}}encoding: [0x9b]
# ASM: mov r3, a{{.*}}encoding: [0xfb]
# ASM: inc r3{{.*}}encoding: [0x0b]
# ASM: mov a, r0{{.*}}encoding: [0xe8]
# ASM: mov a, r7{{.*}}encoding: [0xef]
# ASM: inc r7{{.*}}encoding: [0x0f]
# ASM: dec r3{{.*}}encoding: [0x1b]
# ASM: mov r4, #90{{.*}}encoding: [0x7c,0x5a]
# ASM: mov 32, 33{{.*}}encoding: [0x85,0x21,0x20]
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
# ASM: dec @r0{{.*}}encoding: [0x16]
# ASM: dec @r1{{.*}}encoding: [0x17]
# ASM: mov c, 32{{.*}}encoding: [0xa2,0x20]
# ASM: mov 32, c{{.*}}encoding: [0x92,0x20]
# ASM: clr 32{{.*}}encoding: [0xc2,0x20]
# ASM: setb 32{{.*}}encoding: [0xd2,0x20]
# ASM: cpl 32{{.*}}encoding: [0xb2,0x20]
# ASM: anl c, 32{{.*}}encoding: [0x82,0x20]
# ASM: anl c, /32{{.*}}encoding: [0xb0,0x20]
# ASM: orl c, 32{{.*}}encoding: [0x72,0x20]
# ASM: orl c, /32{{.*}}encoding: [0xa0,0x20]
# ASM: xch a, r3{{.*}}encoding: [0xcb]
# ASM: xch a, 32{{.*}}encoding: [0xc5,0x20]
# ASM: xch a, @r0{{.*}}encoding: [0xc6]
# ASM: xchd a, @r1{{.*}}encoding: [0xd7]
# ASM: cjne a, #18, {{.*}}encoding: [0xb4,0x12,{{[^]]+}}]
# ASM: cjne a, 32, {{.*}}encoding: [0xb5,0x20,{{[^]]+}}]
# ASM: cjne @r0, #18, {{.*}}encoding: [0xb6,0x12,{{[^]]+}}]
# ASM: cjne r3, #18, {{.*}}encoding: [0xbb,0x12,{{[^]]+}}]
# ASM: djnz 32, {{.*}}encoding: [0xd5,0x20,{{[^]]+}}]
# ASM: djnz r3, {{.*}}encoding: [0xdb,{{[^]]+}}]
# ASM: add a, 32{{.*}}encoding: [0x25,0x20]
# ASM: addc a, 32{{.*}}encoding: [0x35,0x20]
# ASM: subb a, 32{{.*}}encoding: [0x95,0x20]
# ASM: anl a, 32{{.*}}encoding: [0x55,0x20]
# ASM: orl a, 32{{.*}}encoding: [0x45,0x20]
# ASM: xrl a, 32{{.*}}encoding: [0x65,0x20]
# ASM: add a, @r0{{.*}}encoding: [0x26]
# ASM: addc a, @r1{{.*}}encoding: [0x37]
# ASM: subb a, @r0{{.*}}encoding: [0x96]
# ASM: anl a, @r1{{.*}}encoding: [0x57]
# ASM: orl a, @r0{{.*}}encoding: [0x46]
# ASM: xrl a, @r1{{.*}}encoding: [0x67]
# ASM: anl 32, #18{{.*}}encoding: [0x53,0x20,0x12]
# ASM: orl 32, #18{{.*}}encoding: [0x43,0x20,0x12]
# ASM: xrl 32, #18{{.*}}encoding: [0x63,0x20,0x12]
# ASM: mov r3, 32{{.*}}encoding: [0xab,0x20]
# ASM: mov 32, r3{{.*}}encoding: [0x8b,0x20]
# ASM: mov @r0, 32{{.*}}encoding: [0xa6,0x20]
# ASM: mov @r1, 32{{.*}}encoding: [0xa7,0x20]
# ASM: mov 32, @r0{{.*}}encoding: [0x86,0x20]
# ASM: mov 32, @r1{{.*}}encoding: [0x87,0x20]
# ASM: anl 32, a{{.*}}encoding: [0x52,0x20]
# ASM: orl 32, a{{.*}}encoding: [0x42,0x20]
# ASM: xrl 32, a{{.*}}encoding: [0x62,0x20]
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
# DIS: mov 240, r3
# DIS: add a, r3
# DIS: subb a, r3
# DIS: mov r3, a
# DIS: inc r3
# DIS: mov a, r0
# DIS: mov a, r7
# DIS: inc r7
# DIS: dec r3
# DIS: mov r4, #90
# DIS: mov 32, 33
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
# DIS: dec @r0
# DIS: dec @r1
# DIS: mov c, 32
# DIS: mov 32, c
# DIS: clr 32
# DIS: setb 32
# DIS: cpl 32
# DIS: anl c, 32
# DIS: anl c, /32
# DIS: orl c, 32
# DIS: orl c, /32
# DIS: xch a, r3
# DIS: xch a, 32
# DIS: xch a, @r0
# DIS: xchd a, @r1
# DIS: cjne a, #18,
# DIS: cjne a, 32,
# DIS: cjne @r0, #18,
# DIS: cjne r3, #18,
# DIS: djnz 32,
# DIS: djnz r3,
# DIS: add a, 32
# DIS: addc a, 32
# DIS: subb a, 32
# DIS: anl a, 32
# DIS: orl a, 32
# DIS: xrl a, 32
# DIS: add a, @r0
# DIS: addc a, @r1
# DIS: subb a, @r0
# DIS: anl a, @r1
# DIS: orl a, @r0
# DIS: xrl a, @r1
# DIS: anl 32, #18
# DIS: orl 32, #18
# DIS: xrl 32, #18
# DIS: mov r3, 32
# DIS: mov 32, r3
# DIS: mov @r0, 32
# DIS: mov @r1, 32
# DIS: mov 32, @r0
# DIS: mov 32, @r1
# DIS: anl 32, a
# DIS: orl 32, a
# DIS: xrl 32, a
# DIS: ret
# DIS: reti
