.intel_syntax noprefix
.globl scheduler_switch

.section .text
scheduler_switch:
    /* Save callee-saved registers */
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    /* Save the stack pointer */
    mov [rdi], rsp

    /* Load the new stack pointer */
    mov rsp, rsi

    /* Restore callee-saved registers */
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret
