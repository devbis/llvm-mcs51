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
  mov sp, #127

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

  /* Bit-address-space globals use bit addresses 0 through 127. Their backing
     bytes are in the just-cleared 0x20-0x2f internal DATA window. */
  mov dptr, #__mcs51_bit_start
  mov r2, dpl
  mov dptr, #__mcs51_bit_end
  mov r3, dpl
  mov dptr, #__mcs51_bit_load
  lcall .Lcopy_initialized_bits

  /* Copy initialized XDATA globals from their CODE load image. */
  mov dptr, #__mcs51_data_start
  mov r2, dpl
  mov r3, dph
  mov dptr, #__mcs51_data_end
  mov r4, dpl
  mov r5, dph
  mov dptr, #__mcs51_data_load
  lcall .Lcopy_initialized_data

  /* PDATA objects occupy page zero selected by the reset value of MPAGE. */
  mov dptr, #__mcs51_pdata_start
  mov r2, dpl
  mov r3, dph
  mov dptr, #__mcs51_pdata_end
  mov r4, dpl
  mov r5, dph
  mov dptr, #__mcs51_pdata_load
  lcall .Lcopy_initialized_data

  /* Copy initialized DATA-space and IDATA-space globals into their SRAM
     alias. The helper skips empty ranges. */
  mov dptr, #__mcs51_data1_xdata_start
  mov r2, dpl
  mov r3, dph
  mov dptr, #__mcs51_data1_xdata_end
  mov r4, dpl
  mov r5, dph
  mov dptr, #__mcs51_data1_load
  lcall .Lcopy_initialized_data

  mov dptr, #__mcs51_data2_xdata_start
  mov r2, dpl
  mov r3, dph
  mov dptr, #__mcs51_data2_xdata_end
  mov r4, dpl
  mov r5, dph
  mov dptr, #__mcs51_data2_load
  lcall .Lcopy_initialized_data

  lcall main
.Lhalt:
  sjmp .Lhalt
.size __mcs51_start, .-__mcs51_start

.Lcopy_initialized_data:
  mov r0, dpl
  mov r1, dph
.Lcopy_data_loop:
  mov a, r2
  xrl a, r4
  jnz .Lcopy_data_byte
  mov a, r3
  xrl a, r5
  jz .Lcopy_data_done
.Lcopy_data_byte:
  mov dpl, r0
  mov dph, r1
  clr a
  movc a, @a+dptr
  mov r6, a
  inc r0
  cjne r0, #0, .Lsource_no_carry
  inc r1
.Lsource_no_carry:
  mov dpl, r2
  mov dph, r3
  mov a, r6
  movx @dptr, a
  inc dptr
  mov r2, dpl
  mov r3, dph
  sjmp .Lcopy_data_loop
.Lcopy_data_done:
  ret

.Lcopy_initialized_bits:
  mov r1, #1
.Lcopy_bit_loop:
  mov a, r2
  xrl a, r3
  jz .Lcopy_bits_done
  clr a
  movc a, @a+dptr
  anl a, #1
  mov r6, a
  inc dptr

  /* Convert bit address N into DATA byte 0x20 + N/8 and mask 1 << N%8. */
  mov a, r2
  anl a, #7
  mov r5, a
  mov a, r2
  rr a
  rr a
  rr a
  anl a, #0x1f
  add a, #0x20
  mov r0, a
  mov a, r5
  jz .Lbit_mask_ready
.Lmake_bit_mask:
  mov a, r1
  rl a
  mov r1, a
  dec r5
  mov a, r5
  jnz .Lmake_bit_mask
.Lbit_mask_ready:
  mov a, r6
  jz .Lclear_initialized_bit
  mov a, @r0
  orl a, r1
  sjmp .Lstore_initialized_bit
.Lclear_initialized_bit:
  mov a, r1
  cpl a
  mov r1, a
  mov a, @r0
  anl a, r1
.Lstore_initialized_bit:
  mov @r0, a
  mov r1, #1
  inc r2
  sjmp .Lcopy_bit_loop
.Lcopy_bits_done:
  ret
