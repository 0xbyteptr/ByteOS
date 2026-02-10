.section .bss
.align 16
.global kernel_stack
kernel_stack:
    .zero 32768             /* 32 KiB kernel stack */
.global kernel_stack_top
kernel_stack_top = kernel_stack + 32768  /* wskazuje na górę stosu */

.section .text
.global _start
.type _start, @function

_start:
    cli                     /* wyłącz przerwania */
    lea kernel_stack_top(%rip), %rsp  /* ustaw RSP na górę stosu */

    /* inicjalizacja GDT i TSS w C */
    call gdt_init           /* ustawia segmenty */
    call tss_init           /* ustawia TSS i rsp0 */

    /* opcjonalnie: init IDT i inne rzeczy */
    
    call kernel_main        /* przejdź do funkcji głównej kernela */

1:  hlt
    jmp 1b
