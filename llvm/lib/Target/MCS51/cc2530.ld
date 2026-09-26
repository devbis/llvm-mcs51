/* CC2530 root-bank memory map for firmware that fits the common flash area.
 * CODE and XDATA have independent address buses, so their VMAs overlap. */
ENTRY(reset)

MEMORY
{
  CODE      (rx) : ORIGIN = 0x0000, LENGTH = 0x8000
  XRAM      (rw) : ORIGIN = 0x0000, LENGTH = 0x2000
  DATA_RAM  (rw) : ORIGIN = 0x0000, LENGTH = 0x0100
  BIT_RAM   (rw) : ORIGIN = 0x0000, LENGTH = 0x0080
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
  .vectors 0x0000 : { KEEP(*(.vectors)) } > CODE

  .mcs51_vector_0 0x0003 : { KEEP(*(.mcs51.vector.0)) } > CODE
  .mcs51_vector_1 0x000b : { KEEP(*(.mcs51.vector.1)) } > CODE
  .mcs51_vector_2 0x0013 : { KEEP(*(.mcs51.vector.2)) } > CODE
  .mcs51_vector_3 0x001b : { KEEP(*(.mcs51.vector.3)) } > CODE
  .mcs51_vector_4 0x0023 : { KEEP(*(.mcs51.vector.4)) } > CODE
  .mcs51_vector_5 0x002b : { KEEP(*(.mcs51.vector.5)) } > CODE
  .mcs51_vector_6 0x0033 : { KEEP(*(.mcs51.vector.6)) } > CODE
  .mcs51_vector_7 0x003b : { KEEP(*(.mcs51.vector.7)) } > CODE
  .mcs51_vector_8 0x0043 : { KEEP(*(.mcs51.vector.8)) } > CODE
  .mcs51_vector_9 0x004b : { KEEP(*(.mcs51.vector.9)) } > CODE
  .mcs51_vector_10 0x0053 : { KEEP(*(.mcs51.vector.10)) } > CODE
  .mcs51_vector_11 0x005b : { KEEP(*(.mcs51.vector.11)) } > CODE
  .mcs51_vector_12 0x0063 : { KEEP(*(.mcs51.vector.12)) } > CODE
  .mcs51_vector_13 0x006b : { KEEP(*(.mcs51.vector.13)) } > CODE
  .mcs51_vector_14 0x0073 : { KEEP(*(.mcs51.vector.14)) } > CODE
  .mcs51_vector_15 0x007b : { KEEP(*(.mcs51.vector.15)) } > CODE
  .mcs51_vector_16 0x0083 : { KEEP(*(.mcs51.vector.16)) } > CODE
  .mcs51_vector_17 0x008b : { KEEP(*(.mcs51.vector.17)) } > CODE

  /* CC2530 vector slots occupy 0x0003 through 0x008d (18 vectors). */
  .text 0x0094 :
  {
    *(.text .text.*)
    *(.rodata .rodata.*)
  } > CODE

  /*
   * Banked code executes at the same 0x8000-0xffff CPU address range, but
   * each output section has a distinct physical flash load address. Put code
   * in these banks with input sections such as .bank1.text and .bank2.rodata.
   * Cross-bank direct calls use root-bank trampolines.
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

  /* The bit address space uses 8-bit bit addresses, independently of DATA. */
  .mcs51_bit 0 :
  {
    __mcs51_bit_start = .;
    *(.mcs51.bit)
    __mcs51_bit_end = .;
  } > BIT_RAM AT> CODE
  __mcs51_bit_load = LOADADDR(.mcs51_bit);
  .mcs51_bit_bss (NOLOAD) :
  {
    *(.mcs51.bit.bss)
    __mcs51_bit_bss_end = .;
  } > BIT_RAM
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
ASSERT(__mcs51_bit_bss_end <= LENGTH(BIT_RAM),
       "CC2530 bit globals exceed the bit-addressable DATA area")
ASSERT(ADDR(.text) + SIZEOF(.text) <= 0x8000,
       "CC2530 common code exceeds the 32-KiB root bank")
ASSERT(SIZEOF(.mcs51_vector_0) <= 3,
       "CC2530 interrupt vector 0 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_1) <= 3,
       "CC2530 interrupt vector 1 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_2) <= 3,
       "CC2530 interrupt vector 2 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_3) <= 3,
       "CC2530 interrupt vector 3 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_4) <= 3,
       "CC2530 interrupt vector 4 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_5) <= 3,
       "CC2530 interrupt vector 5 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_6) <= 3,
       "CC2530 interrupt vector 6 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_7) <= 3,
       "CC2530 interrupt vector 7 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_8) <= 3,
       "CC2530 interrupt vector 8 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_9) <= 3,
       "CC2530 interrupt vector 9 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_10) <= 3,
       "CC2530 interrupt vector 10 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_11) <= 3,
       "CC2530 interrupt vector 11 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_12) <= 3,
       "CC2530 interrupt vector 12 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_13) <= 3,
       "CC2530 interrupt vector 13 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_14) <= 3,
       "CC2530 interrupt vector 14 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_15) <= 3,
       "CC2530 interrupt vector 15 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_16) <= 3,
       "CC2530 interrupt vector 16 has multiple handlers")
ASSERT(SIZEOF(.mcs51_vector_17) <= 3,
       "CC2530 interrupt vector 17 has multiple handlers")
ASSERT(SIZEOF(.bank1) <= LENGTH(XDATA_BANK), "CC2530 bank 1 exceeds 32 KiB")
ASSERT(SIZEOF(.bank2) <= LENGTH(XDATA_BANK), "CC2530 bank 2 exceeds 32 KiB")
ASSERT(SIZEOF(.bank3) <= LENGTH(XDATA_BANK), "CC2530 bank 3 exceeds 32 KiB")
ASSERT(SIZEOF(.bank4) <= LENGTH(XDATA_BANK), "CC2530 bank 4 exceeds 32 KiB")
ASSERT(SIZEOF(.bank5) <= LENGTH(XDATA_BANK), "CC2530 bank 5 exceeds 32 KiB")
ASSERT(SIZEOF(.bank6) <= LENGTH(XDATA_BANK), "CC2530 bank 6 exceeds 32 KiB")
ASSERT(SIZEOF(.bank7) <= LENGTH(XDATA_BANK), "CC2530 bank 7 exceeds 32 KiB")
ASSERT(SIZEOF(.bank7) == 0 || 0x38000 + SIZEOF(.bank7) <= 0x40000,
       "CC2530 banked image exceeds 256-KiB flash")
