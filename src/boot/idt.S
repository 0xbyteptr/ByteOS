.intel_syntax noprefix
.global __load_idt_asm
.global __isr_stub_80
.global __isr_stub_14
.global __isr_panic

.section .rodata
idt_msg: .asciz "idt.S: enter __load_idt_asm\n"
idt_msg_done: .asciz "idt.S: after lidt\n"

.section .text
.type __load_idt_asm, @function
__load_idt_asm:
    push rbx
    mov rbx, rdi
    lea rdi, [rip + idt_msg]
    call serial_puts
    mov rdi, rbx
    mov rax, rdi
    lidt [rax]
    lea rdi, [rip + idt_msg_done]
    call serial_puts
    pop rbx
    ret

/* isr_stub_80: interrupt entry for int 0x80
   This stub saves general registers, calls the C handler, restores registers and iretq. */
.type __isr_stub_80, @function
__isr_stub_80:
    /* save caller-saved registers and callee-saved we'll restore after */
    push rbp
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rdx
    push rcx
    push rbx
    push rax

    /* pass registers to C handler: rdi = rax (syscall num), rsi = rdi (arg1), rdx = rsi (arg2), rcx = rdx (arg3) */
    mov rdi, [rsp + 8*0] /* saved rax */
    mov rsi, [rsp + 8*4] /* saved rdi */
    mov rdx, [rsp + 8*5] /* saved rsi */
    mov rcx, [rsp + 8*3] /* saved rdx */
    call syscall_handler

    /* restore registers (reverse) */
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbp

    iretq
    .size __isr_stub_80, .-__isr_stub_80


/* isr_stub_14: page fault handler */
.type __isr_stub_14, @function
__isr_stub_14:
    /* save registers */
    push rbp
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rdx
    push rcx
    push rbx
    push rax

    /* cr2 has the faulting address */
    mov rdi, cr2
    call page_fault_handler

    /* restore */
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbp

    iretq
    .size __isr_stub_14, .-__isr_stub_14


/* __isr_panic: default handler used by early IDT entries; it halts the CPU */
.type __isr_panic, @function
__isr_panic:
    /* minimal panic handler: disable interrupts and halt indefinitely */
    cli
1:  hlt
    jmp 1b
    .size __isr_panic, .-__isr_panic
