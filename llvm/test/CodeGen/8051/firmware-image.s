# RUN: llvm-mc -triple=mcs51 -filetype=obj %s -o %t.o
# RUN: ld.lld -m elf32-mcs51 --entry=reset -Ttext=0 %t.o -o %t.elf
# RUN: llvm-objcopy -O ihex %t.elf %t.hex
# RUN: FileCheck %s --check-prefix=HEX < %t.hex

.section .text,"ax"
.globl reset
.type reset,@function
reset:
  nop
  ret
.size reset, .-reset

# HEX: :020000000022DC
# HEX-NEXT: :00000001FF
