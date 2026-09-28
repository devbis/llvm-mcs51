# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 --section-start=.data=0x100 -e _start %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s

        .text
        .globl _start
_start:
        mov     dptr, #a
        movx    @dptr, a
        mov     dptr, #b
        movx    @dptr, a
        ret

skipped:
        mov     dptr, #a
        sjmp    .Lafter
        mov     dptr, #b
.Lafter:
        mov     dptr, #c
        ret

        .data
a:      .byte   0
b:      .byte   0
c:      .byte   0

# CHECK-LABEL: <_start>:
# CHECK: inc dptr
# CHECK-LABEL: <skipped>:
# CHECK: mov dptr, #256
# CHECK: sjmp
# CHECK: mov dptr, #257
# CHECK: mov dptr, #258
