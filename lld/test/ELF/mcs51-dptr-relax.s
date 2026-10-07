# RUN: llvm-mc -triple=mcs51 -mattr=+relax -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 --section-start=.data=0x100 -e _start %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s
# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.default.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 --section-start=.data=0x100 -e _start %t.default.o -o %t.default
# RUN: llvm-objdump -d %t.default | FileCheck %s --check-prefix=DEFAULT

## Relaxation is opt-in (-mattr=+relax); default objects are never shrunk.

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

# DEFAULT-LABEL: <_start>:
# DEFAULT-NEXT: mov dptr, #256
# DEFAULT-NEXT: movx @dptr, a
# DEFAULT-NEXT: mov dptr, #257
