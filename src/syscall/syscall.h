#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3);
#endif
