/* CC2530 root-bank memory map for firmware that fits the common flash area.
 * CODE and XDATA have independent address buses, so their VMAs overlap. */
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

  /* Initial values are stored in flash; startup code must copy them to XDATA. */
  .data : { *(.data .data.*) } > XDATA AT> CODE
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } > XDATA
}
