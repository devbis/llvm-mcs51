ENTRY(reset)

MEMORY
{
  CODE (rx) : ORIGIN = 0, LENGTH = 0x1000
  XRAM (rw) : ORIGIN = 0, LENGTH = 0x1000
}

SECTIONS
{
  .text 0x200 : { *(.text .text.*) } > CODE
  .code_table 0x100 : { *(.mcs51.code.rodata .mcs51.code.rodata.*) } > CODE
  .data 0x100 :
  {
    __mcs51_data_start = .;
    *(.mcs51.xdata.init .mcs51.xdata.init.*)
    __mcs51_data_end = .;
  } > XRAM AT> CODE
  __mcs51_data_load = LOADADDR(.data);
  .bss (NOLOAD) :
  {
    __mcs51_bss_start = .;
    *(.mcs51.xdata.bss .mcs51.xdata.bss.*)
    __mcs51_bss_end = .;
  } > XRAM
}
