SECTIONS
{
  . = 0x80000;

  .text : {
    *(.text)
  }

  .rodata : { *(.rodata*) }

  .data : { *(.data*) }

  . = ALIGN(8);
  __bss_start = .;
  .bss : {
    *(.bss*)
    *(COMMON)
  }
  __bss_end = .;
}

_stack_top = 0x80000;
