# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

        nop
        inc     a
        dec     r7
        inc     @r0
        mov     a, #0x5a
        mov     r3, #0x12
        mov     @r1, a
        mov     a, @r0
        mov     dptr, #0x1234
        ljmp    0x1234
        lcall   0x1234
        inc     dptr
        movc    a, @a+dptr
        movx    a, @dptr
        movx    @dptr, a
        add     a, #1
        subb    a, #1
        anl     0x20, #1
        orl     c, 0x20
        xch     a, 0x20
        clr     0x20
        push    0x81
        pop     0x81
        ajmp    0x234
        acall   0x234
        addc    a, @r1
        mov     0x20, 0x21
        mov     0x20, @r0
        mov     @r1, 0x20
        anl     c, /0x20
        orl     c, /0x20
        mov     c, 0x20
        mov     0x20, c
        xchd    a, @r0
        xchd    a, @r1
        cjne    a, #1, branch_target
        cjne    a, 0x20, branch_target
        cjne    @r0, #1, branch_target
        cjne    r7, #1, branch_target
        djnz    0x20, branch_target
        djnz    r7, branch_target
        mov     @r0, #0x55
        mov     @r1, #0xaa
        rr      a
        rl      a
        setb    c
branch_target:
        ret
        reti

# CHECK: nop{{.*}}encoding: [0x00]
# CHECK: inc a{{.*}}encoding: [0x04]
# CHECK: dec r7{{.*}}encoding: [0x1f]
# CHECK: inc @r0{{.*}}encoding: [0x06]
# CHECK: mov a, #90{{.*}}encoding: [0x74,0x5a]
# CHECK: mov r3, #18{{.*}}encoding: [0x7b,0x12]
# CHECK: mov @r1, a{{.*}}encoding: [0xf7]
# CHECK: mov a, @r0{{.*}}encoding: [0xe6]
# CHECK: mov dptr, #4660{{.*}}encoding: [0x90,0x12,0x34]
# CHECK: ljmp 4660{{.*}}encoding: [0x02,0x12,0x34]
# CHECK: lcall 4660{{.*}}encoding: [0x12,0x12,0x34]
# CHECK: inc dptr{{.*}}encoding: [0xa3]
# CHECK: movc a, @a+dptr{{.*}}encoding: [0x93]
# CHECK: movx a, @dptr{{.*}}encoding: [0xe0]
# CHECK: movx @dptr, a{{.*}}encoding: [0xf0]
# CHECK: add a, #1{{.*}}encoding: [0x24,0x01]
# CHECK: subb a, #1{{.*}}encoding: [0x94,0x01]
# CHECK: anl 32, #1{{.*}}encoding: [0x53,0x20,0x01]
# CHECK: orl c, 32{{.*}}encoding: [0x72,0x20]
# CHECK: xch a, 32{{.*}}encoding: [0xc5,0x20]
# CHECK: clr 32{{.*}}encoding: [0xc2,0x20]
# CHECK: push 129{{.*}}encoding: [0xc0,0x81]
# CHECK: pop 129{{.*}}encoding: [0xd0,0x81]
# CHECK: ajmp 564{{.*}}encoding: [0x41,0x34]
# CHECK: acall 564{{.*}}encoding: [0x51,0x34]
# CHECK: addc a, @r1{{.*}}encoding: [0x37]
# CHECK: mov 32, 33{{.*}}encoding: [0x85,0x21,0x20]
# CHECK: mov 32, @r0{{.*}}encoding: [0x86,0x20]
# CHECK: mov @r1, 32{{.*}}encoding: [0xa7,0x20]
# CHECK: anl c, /32{{.*}}encoding: [0xb0,0x20]
# CHECK: orl c, /32{{.*}}encoding: [0xa0,0x20]
# CHECK: mov c, 32{{.*}}encoding: [0xa2,0x20]
# CHECK: mov 32, c{{.*}}encoding: [0x92,0x20]
# CHECK: xchd a, @r0{{.*}}encoding: [0xd6]
# CHECK: xchd a, @r1{{.*}}encoding: [0xd7]
# CHECK: cjne a, #1{{.*}}encoding: [0xb4,0x01,{{.*}}]
# CHECK: cjne a, 32{{.*}}encoding: [0xb5,0x20,{{.*}}]
# CHECK: cjne @r0, #1{{.*}}encoding: [0xb6,0x01,{{.*}}]
# CHECK: cjne r7, #1{{.*}}encoding: [0xbf,0x01,{{.*}}]
# CHECK: djnz 32{{.*}}encoding: [0xd5,0x20,{{.*}}]
# CHECK: djnz r7{{.*}}encoding: [0xdf,{{.*}}]
# CHECK: mov @r0, #85{{.*}}encoding: [0x76,0x55]
# CHECK: mov @r1, #170{{.*}}encoding: [0x77,0xaa]
# CHECK: rr a{{.*}}encoding: [0x03]
# CHECK: rl a{{.*}}encoding: [0x23]
# CHECK: setb c{{.*}}encoding: [0xd3]
# CHECK: ret{{.*}}encoding: [0x22]
# CHECK: reti{{.*}}encoding: [0x32]

# DIS: mov dptr, #4660
# DIS: ljmp 4660
# DIS: lcall 4660
# DIS: mov 32, 33
# DIS: xchd a, @r0
# DIS: xchd a, @r1
# DIS: cjne a, #1,
# DIS: cjne a, 32,
# DIS: cjne @r0, #1,
# DIS: cjne r7, #1,
# DIS: djnz 32,
# DIS: djnz r7,
# DIS: mov @r0, #85
# DIS: mov @r1, #170
# DIS: rr a
# DIS: rl a
# DIS: setb c
