.intel_syntax noprefix
.globl __load_gdt_asm
.globl __load_tr_asm

.section .rodata
gdt_msg: .asciz "gdt.S: enter __load_gdt_asm\n"
gdt_msg_done: .asciz "gdt.S: after lgdt\n"
tr_msg: .asciz "gdt.S: enter __load_tr_asm\n"
tr_msg_done: .asciz "gdt.S: after ltr\n"

.type __load_gdt_asm, @function
__load_gdt_asm:
    push rdi
    lea rdi, [rip + gdt_msg]
    call serial_puts
    pop rdi

    lgdt [rdi]

    push rdi
    lea rdi, [rip + gdt_msg_done]
    call serial_puts
    pop rdi

    ret

.type __load_tr_asm, @function
__load_tr_asm:
    push rdi
    lea rdi, [rip + tr_msg]
    call serial_puts
    pop rdi

    ltr di

    push rdi
    lea rdi, [rip + tr_msg_done]
    call serial_puts
    pop rdi

    ret
