# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: not ld.lld -m elf32-mcs51 %t.o -o %t 2>&1 | FileCheck %s

# Address-space-tagged relocations let LLD catch cross-bus references even
# when CODE and XDATA use the same numeric address range.

        .section .text.xdata_reader,"ax",@progbits
        mov dptr, #xdata16(code_object)
        movx a, @dptr
        ret

        .section .text.code_reader,"ax",@progbits
        mov dptr, #code16(xdata_object)
        clr a
        movc a, @a+dptr
        ret

        .section .mcs51.code.rodata.code_object,"a",@progbits
        .type code_object,@object
code_object:
        .byte 1
        .size code_object, 1

        .section .mcs51.xdata.bss.xdata_object,"aw",@nobits
        .type xdata_object,@object
xdata_object:
        .space 1
        .size xdata_object, 1

        .section .mcs51.xdata.init.uncopied,"aw",@progbits
uncopied:
        .byte 2

# CHECK-DAG: initialized MCS-51 XDATA section '.mcs51.xdata.init.uncopied' has no startup copy path
# CHECK-DAG: MCS-51 XDATA access from section '.text.xdata_reader' targets symbol 'code_object' in section '.mcs51.code.rodata.code_object' (CODE); CODE and XDATA are separate address spaces
# CHECK-DAG: MCS-51 CODE access from section '.text.code_reader' targets symbol 'xdata_object' in section '.mcs51.xdata.bss.xdata_object' (XDATA); CODE and XDATA are separate address spaces
