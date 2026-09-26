/* Minimal reset/runtime entry for the common-window CC2530 linker profile. */
.section .vectors,"ax"
.globl reset
.type reset,@function
reset:
  ljmp __mcs51_start
.size reset, .-reset

.section .text.startup,"ax"
.globl __mcs51_start
.type __mcs51_start,@function
__mcs51_start:
  /* Keep the hardware stack above register banks and bit-addressable RAM. */
  mov 129, #127

  /* Clear the full 8 KiB XDATA SRAM, including all zero-initialized globals. */
  mov dptr, #0
  mov r6, #0
  mov r7, #32
.Lclear_xdata:
  clr a
  movx @dptr, a
  inc dptr
  dec r6
  cjne r6, #255, .Lcheck_xdata_count
  dec r7
.Lcheck_xdata_count:
  mov a, r6
  orl a, r7
  jnz .Lclear_xdata

  lcall entry
.Lhalt:
  sjmp .Lhalt
.size __mcs51_start, .-__mcs51_start
