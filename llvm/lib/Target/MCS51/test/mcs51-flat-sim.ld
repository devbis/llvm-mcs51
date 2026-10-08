/* Flat MCS-51 test image used to execute the generic-pointer runtime in uCsim. */
ENTRY(main)

MEMORY {
  CODE (rx) : ORIGIN = 0, LENGTH = 0x10000
  XRAM (rw) : ORIGIN = 0, LENGTH = 0x10000
}

SECTIONS {
  .text 0 :
  { KEEP(*(.text.0start)) *(.text .text.*) *(.rodata .rodata.*)
    *(.mcs51.code.rodata .mcs51.code.rodata.*) } > CODE
  .data 0 : { *(.data .data.*) *(.mcs51.xdata.init .mcs51.xdata.init.*) } > XRAM AT> CODE
  .bss (NOLOAD) :
  { *(.bss .bss.*) *(.mcs51.xdata.bss .mcs51.xdata.bss.*) *(COMMON) } > XRAM
  .mcs51_pdata 0 : { *(.mcs51.pdata) } > XRAM AT> CODE
  .mcs51_pdata_bss (NOLOAD) : { *(.mcs51.pdata.bss) } > XRAM
  .mcs51_data1 0x48 : { *(.mcs51.data1) } > XRAM AT> CODE
  .mcs51_data1_bss 0x48 (NOLOAD) : { *(.mcs51.data1.bss) } > XRAM
  .mcs51_data2 0x80 : { *(.mcs51.data2) } > XRAM AT> CODE
  .mcs51_data2_bss 0x80 (NOLOAD) : { *(.mcs51.data2.bss) } > XRAM
  .mcs51_bit 0 : { *(.mcs51.bit) } > XRAM AT> CODE
  .mcs51_bit_bss (NOLOAD) : { *(.mcs51.bit.bss) } > XRAM
  .simresult 0x100 (NOLOAD) : { *(.mcs51.simresult) } > XRAM
}
