OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    .text : { *(.text .text*) }
    .rodata : { *(.rodata .rodata*) }
    .data : { *(.data .data*) }
    .bss : { *(.bss .bss*) *(COMMON) }

    /* keep limine requests in the binary */
    .limine_reqs : { *(.limine_reqs) }
}
