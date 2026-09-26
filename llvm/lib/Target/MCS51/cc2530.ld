/* CC2530 root-bank memory map for firmware that fits the common flash area.
 * CODE and XDATA have independent address buses, so their VMAs overlap. */
ENTRY(reset)

MEMORY
{
  CODE      (rx) : ORIGIN = 0x0000, LENGTH = 0x8000
  XRAM      (rw) : ORIGIN = 0x0000, LENGTH = 0x2000
  XREG      (rw) : ORIGIN = 0x6000, LENGTH = 0x0400
  SFR_XDATA (rw) : ORIGIN = 0x7080, LENGTH = 0x0080
  INFO_PAGE (r)  : ORIGIN = 0x7800, LENGTH = 0x0800
  XDATA_BANK(r)  : ORIGIN = 0x8000, LENGTH = 0x8000
}

/* XDATA aliases for registers and the upper 256 bytes of the SRAM. */
__cc2530_data_alias_start = 0x1f00;
__cc2530_data_alias_end = 0x2000;
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
}
