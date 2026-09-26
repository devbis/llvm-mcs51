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

  /*
   * Banked code executes at the same 0x8000-0xffff CPU address range, but
   * each output section has a distinct physical flash load address. Put code
   * in these banks with input sections such as .bank1.text and .bank2.rodata.
   * A bank-aware call ABI is still required before code may call across banks.
   */
  .bank1 0x8000 : AT(0x08000)
  {
    __cc2530_bank1_start = .; *(.bank1 .bank1.*) __cc2530_bank1_end = .;
  } > XDATA_BANK
  .bank2 0x8000 : AT(0x10000)
  {
    __cc2530_bank2_start = .; *(.bank2 .bank2.*) __cc2530_bank2_end = .;
  } > XDATA_BANK
  .bank3 0x8000 : AT(0x18000)
  {
    __cc2530_bank3_start = .; *(.bank3 .bank3.*) __cc2530_bank3_end = .;
  } > XDATA_BANK
  .bank4 0x8000 : AT(0x20000)
  {
    __cc2530_bank4_start = .; *(.bank4 .bank4.*) __cc2530_bank4_end = .;
  } > XDATA_BANK
  .bank5 0x8000 : AT(0x28000)
  {
    __cc2530_bank5_start = .; *(.bank5 .bank5.*) __cc2530_bank5_end = .;
  } > XDATA_BANK
  .bank6 0x8000 : AT(0x30000)
  {
    __cc2530_bank6_start = .; *(.bank6 .bank6.*) __cc2530_bank6_end = .;
  } > XDATA_BANK
  .bank7 0x8000 : AT(0x38000)
  {
    __cc2530_bank7_start = .; *(.bank7 .bank7.*) __cc2530_bank7_end = .;
  } > XDATA_BANK

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
ASSERT(ADDR(.text) + SIZEOF(.text) <= 0x8000,
       "CC2530 common code exceeds the 32-KiB root bank")
ASSERT(SIZEOF(.bank1) <= LENGTH(XDATA_BANK), "CC2530 bank 1 exceeds 32 KiB")
ASSERT(SIZEOF(.bank2) <= LENGTH(XDATA_BANK), "CC2530 bank 2 exceeds 32 KiB")
ASSERT(SIZEOF(.bank3) <= LENGTH(XDATA_BANK), "CC2530 bank 3 exceeds 32 KiB")
ASSERT(SIZEOF(.bank4) <= LENGTH(XDATA_BANK), "CC2530 bank 4 exceeds 32 KiB")
ASSERT(SIZEOF(.bank5) <= LENGTH(XDATA_BANK), "CC2530 bank 5 exceeds 32 KiB")
ASSERT(SIZEOF(.bank6) <= LENGTH(XDATA_BANK), "CC2530 bank 6 exceeds 32 KiB")
ASSERT(SIZEOF(.bank7) <= LENGTH(XDATA_BANK), "CC2530 bank 7 exceeds 32 KiB")
ASSERT(SIZEOF(.bank7) == 0 || 0x38000 + SIZEOF(.bank7) <= 0x40000,
       "CC2530 banked image exceeds 256-KiB flash")
