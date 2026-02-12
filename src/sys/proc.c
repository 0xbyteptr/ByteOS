#include <stdint.h>
#include "serial/serial.h"

int64_t kernel_spawn_elf_from_path(const char* path)
{
    serial_puts("[stub] spawn ELF: "); serial_puts(path); serial_puts(" → not implemented\n");
    return -1;
}