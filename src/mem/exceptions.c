#include "serial/serial.h"

void page_fault_handler(uint64_t error_code, uint64_t faulting_address)
{
    serial_puts("PAGE FAULT at 0x");
    serial_puthex64(faulting_address);
    serial_puts(" (error=0x");
    serial_puthex64(error_code);
    serial_puts(")\nHalting.\n");
    while (1) __asm__ volatile ("hlt");
}