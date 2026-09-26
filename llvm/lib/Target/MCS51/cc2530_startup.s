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

  /* Copy initialized globals from their CODE load image to XDATA. */
  mov dptr, #__mcs51_data_start
  mov r2, 130
  mov r3, 131
  mov dptr, #__mcs51_data_end
  mov a, 130
  xrl a, r2
  jnz .Ldata_nonempty
  mov a, 131
  xrl a, r3
  jz .Ldata_done
.Ldata_nonempty:
  mov dptr, #__mcs51_data_load
  mov r0, 130
  mov r1, 131
  mov dptr, #__mcs51_data_end
  mov r4, 130
  mov r5, 131
.Lcopy_data:
  mov 130, r0
  mov 131, r1
  clr a
  movc a, @a+dptr
  mov r6, a
  inc r0
  cjne r0, #0, .Lsource_no_carry
  inc r1
.Lsource_no_carry:
  mov 130, r2
  mov 131, r3
  mov a, r6
  movx @dptr, a
  inc dptr
  mov r2, 130
  mov r3, 131
  mov a, r2
  xrl a, r4
  jnz .Lcopy_data
  mov a, r3
  xrl a, r5
  jnz .Lcopy_data
.Ldata_done:

  lcall main
.Lhalt:
  sjmp .Lhalt
.size __mcs51_start, .-__mcs51_start
