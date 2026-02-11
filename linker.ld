OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    _text_start = .;
    .text : { *(.text .text*) }
    _text_end = .;

    _rodata_start = .;
    .rodata : { *(.rodata .rodata*) }
    _rodata_end = .;

    _data_start = .;
    .data : { *(.data .data*) }
    _data_end = .;

    _bss_start = .;
    .bss : { *(.bss .bss*) *(COMMON) }
    _bss_end = .;

    /* keep limine requests in the binary */
    .limine_reqs : { *(.limine_reqs) }
}
