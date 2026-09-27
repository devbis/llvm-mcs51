# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s

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
# CHECK: ret{{.*}}encoding: [0x22]
# CHECK: reti{{.*}}encoding: [0x32]
