OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(start)

SECTIONS
{
  . = PAYLOAD_ADDR;

  .text     : { *(.text.start) *(.text   .text.*   .gnu.linkonce.t.*) }
  .rodata   : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .data     : { *(.data   .data.*   .gnu.linkonce.d.*) }
  . = ALIGN(4);
  .bss      : {
    __bss_start = .;
    *(.bss    .bss.*    .gnu.linkonce.b.*) *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  }
  /DISCARD/ : { *(.interp) *(.dynsym) *(.dynstr) *(.hash) *(.dynamic) *(.comment) }
}
