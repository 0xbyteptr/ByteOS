.intel_syntax noprefix
.globl scheduler_switch

.section .text
scheduler_switch:
    /* Save callee-saved registers */
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp

    /* Save the stack pointer */
    mov [rdi], rsp

    /* Load the new stack pointer */
    mov rsp, rsi

    /* Restore callee-saved registers */
    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15

    ret
