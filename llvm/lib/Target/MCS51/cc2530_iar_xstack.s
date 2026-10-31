/* IAR ABI stack state for the CC2530 startup profile. The stack grows down
   from 0x1f00 and reserves 512 bytes at the top of the common XDATA SRAM. */
.globl "?V0"
.set "?V0", 0x30
.globl "?V1"
.set "?V1", 0x31
.globl "?V2"
.set "?V2", 0x32
.globl "?V3"
.set "?V3", 0x33
.globl "?V4"
.set "?V4", 0x34
.globl "?V5"
.set "?V5", 0x35
.globl "?V6"
.set "?V6", 0x36
.globl "?V7"
.set "?V7", 0x37

.section .mcs51.data1.bss,"aw",@nobits
.weak "?XSP"
"?XSP":
  .space 2

/* Callee-saved registers of IAR-ABI functions are kept in a one-page XDATA
   save stack (see the __mcs51_save_iar_* helpers). ?CSP is its 8-bit offset
   within that page; the stack grows down from the end of the page. */
.weak "?CSP"
"?CSP":
  .space 1
.globl __mcs51_iar_csr_page
.set __mcs51_iar_csr_page, 0x1c

.section .mcs51.iar.csr,"aw",@nobits
.globl __mcs51_iar_csr_storage
__mcs51_iar_csr_storage:
  .space 256

.section .mcs51.iar.xstack,"aw",@nobits
.globl __mcs51_iar_xstack_storage
__mcs51_iar_xstack_storage:
  .space 512

.section .mcs51.common.iar.xstack.init,"ax",@progbits
.globl __mcs51_iar_xstack_init
.type __mcs51_iar_xstack_init,@function
__mcs51_iar_xstack_init:
  mov dptr, #__mcs51_iar_xstack_end
  mov a, dpl
  mov "?XSP", a
  mov a, dph
  mov "?XSP"+1, a
  clr a
  mov "?CSP", a
  ret
.size __mcs51_iar_xstack_init, .-__mcs51_iar_xstack_init
