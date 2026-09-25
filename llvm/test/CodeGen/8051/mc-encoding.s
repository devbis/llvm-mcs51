# RUN: llvm-mc -triple=mcs51 -show-encoding %s | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

        nop
        inc a
        rl a
        mul ab
        movx a, @dptr
        ret
        reti

# ASM: nop{{.*}}encoding: [0x00]
# ASM: inc a{{.*}}encoding: [0x04]
# ASM: rl a{{.*}}encoding: [0x23]
# ASM: mul ab{{.*}}encoding: [0xa4]
# ASM: movx a, @dptr{{.*}}encoding: [0xe0]
# ASM: ret{{.*}}encoding: [0x22]
# ASM: reti{{.*}}encoding: [0x32]
# DIS: nop
# DIS: inc a
# DIS: rl a
# DIS: mul ab
# DIS: movx a, @dptr
# DIS: ret
# DIS: reti
