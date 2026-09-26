/* CC2530 root-bank memory map for firmware that fits the common flash area.
 * CODE and XDATA have independent address buses, so their VMAs overlap. */
ENTRY(reset)

MEMORY
{
  CODE      (rx) : ORIGIN = 0x0000, LENGTH = 0x8000
  XRAM      (rw) : ORIGIN = 0x0000, LENGTH = 0x2000
  DATA_RAM  (rw) : ORIGIN = 0x0000, LENGTH = 0x0100
  XREG      (rw) : ORIGIN = 0x6000, LENGTH = 0x0400
  SFR_XDATA (rw) : ORIGIN = 0x7080, LENGTH = 0x0080
  INFO_PAGE (r)  : ORIGIN = 0x7800, LENGTH = 0x0800
  XDATA_BANK(r)  : ORIGIN = 0x8000, LENGTH = 0x8000
}

/* XDATA aliases for registers and the upper 256 bytes of the SRAM. */
__cc2530_data_alias_start = 0x1f00;
__cc2530_data_alias_end = 0x2000;
__cc2530_static_data_start = 0x30;
__cc2530_static_data_end = 0x80;
__cc2530_xreg_start = ORIGIN(XREG);
__cc2530_xreg_end = ORIGIN(XREG) + LENGTH(XREG);
__cc2530_sfr_xdata_start = ORIGIN(SFR_XDATA);
__cc2530_sfr_xdata_end = ORIGIN(SFR_XDATA) + LENGTH(SFR_XDATA);
__cc2530_info_start = ORIGIN(INFO_PAGE);
__cc2530_info_end = ORIGIN(INFO_PAGE) + LENGTH(INFO_PAGE);
__cc2530_xbank_start = ORIGIN(XDATA_BANK);
__cc2530_xbank_end = ORIGIN(XDATA_BANK) + LENGTH(XDATA_BANK);

SECTIONS
{
  .text :
  {
    KEEP(*(.vectors))
    *(.text .text.*)
    *(.rodata .rodata.*)
  } > CODE

  /* Initial values are stored in flash and copied to XDATA by reset startup. */
  .data :
  {
    __mcs51_data_start = .;
    *(.data .data.*)
    __mcs51_data_end = .;
  } > XRAM AT> CODE
  __mcs51_data_load = LOADADDR(.data);
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } > XRAM

  /* Address-space 1 and 2 globals occupy the CPU DATA alias of XRAM. */
  .mcs51_data1 __cc2530_static_data_start :
  {
    __mcs51_data1_start = .;
    *(.mcs51.data1)
    __mcs51_data1_end = .;
  } > DATA_RAM AT> CODE
  __mcs51_data1_xdata_start = ADDR(.mcs51_data1) + __cc2530_data_alias_start;
  __mcs51_data1_xdata_end = ADDR(.mcs51_data1) + SIZEOF(.mcs51_data1) +
                            __cc2530_data_alias_start;
  __mcs51_data1_load = LOADADDR(.mcs51_data1);
  .mcs51_bss1 (NOLOAD) :
  {
    *(.mcs51.data1.bss)
    __mcs51_data1_bss_end = .;
  } > DATA_RAM

  .mcs51_data2 :
  {
    __mcs51_data2_start = .;
    *(.mcs51.data2)
    __mcs51_data2_end = .;
  } > DATA_RAM AT> CODE
  __mcs51_data2_xdata_start = ADDR(.mcs51_data2) + __cc2530_data_alias_start;
  __mcs51_data2_xdata_end = ADDR(.mcs51_data2) + SIZEOF(.mcs51_data2) +
                            __cc2530_data_alias_start;
  __mcs51_data2_load = LOADADDR(.mcs51_data2);
  .mcs51_bss2 (NOLOAD) :
  {
    *(.mcs51.data2.bss)
    __mcs51_data2_bss_end = .;
  } > DATA_RAM
}

/* The upper 256 bytes of SRAM are also the CPU's DATA/IDATA address window. */
ASSERT(ADDR(.data) + SIZEOF(.data) <= __cc2530_data_alias_start,
       "CC2530 .data overlaps the DATA-space SRAM alias")
ASSERT(ADDR(.bss) + SIZEOF(.bss) <= __cc2530_data_alias_start,
       "CC2530 .bss overlaps the DATA-space SRAM alias")
ASSERT(__mcs51_data1_bss_end <= __cc2530_static_data_end,
       "CC2530 DATA globals exceed directly addressable internal RAM")
ASSERT(__mcs51_data2_bss_end <= __cc2530_static_data_end,
       "CC2530 DATA/IDATA globals overlap the hardware stack")
