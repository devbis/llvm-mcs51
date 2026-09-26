/* CC2530 root-bank memory map for firmware that fits the common flash area.
 * CODE and XDATA have independent address buses, so their VMAs overlap. */
ENTRY(reset)

MEMORY
{
  CODE  (rx) : ORIGIN = 0x0000, LENGTH = 0x8000
  XDATA (rw) : ORIGIN = 0x0000, LENGTH = 0x2000
}

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
  } > XDATA AT> CODE
  __mcs51_data_load = LOADADDR(.data);
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } > XDATA
}
