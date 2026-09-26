# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s

        mov     sp, #127
        mov     dpl, r0
        mov     r1, dph
        mov     a, psw
        mov     b, #0

# CHECK: mov 129, #127{{.*}}encoding: [0x75,0x81,0x7f]
# CHECK: mov 130, r0{{.*}}encoding: [0x88,0x82]
# CHECK: mov r1, 131{{.*}}encoding: [0xa9,0x83]
# CHECK: mov a, 208{{.*}}encoding: [0xe5,0xd0]
# CHECK: mov 240, #0{{.*}}encoding: [0x75,0xf0,0x00]
