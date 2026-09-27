# RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 -Ttext=0 -e entry \
# RUN:   --defsym=byte_symbol=0x20 --defsym=code_symbol=0x1234 \
# RUN:   --defsym=branch_symbol=0x20 %t.o -o %t
# RUN: llvm-readobj --hex-dump=.text %t | FileCheck %s
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RUN: llvm-objcopy --output-target=ihex %t %t.hex
# RUN: FileCheck %s --check-prefix=IHEX < %t.hex

        .text
        .globl entry
entry:
        mov     a, #byte_symbol
        ljmp    code_symbol
        lcall   code_symbol
        sjmp    branch_symbol
        jb      0x20, branch_symbol

        .globl byte_symbol
        .globl code_symbol
        .globl branch_symbol

# CHECK: 0x00000000 74200212 34121234 80162020 13

# DIS: sjmp 32
# DIS: jb 32, 32

# IHEX: :0D00000074200212341212348016202013D6
# IHEX-NEXT: :00000001FF
