#include "compat/panic.h"
#include "serial/serial.h"
#include <stdint.h>

void panic(const char* msg)
{
  serial_puts("kernel: PANIC: ");
  serial_puts(msg);
  serial_puts("\n");

  /* also write to VGA text mode for visibility */
  volatile unsigned short* vga = (unsigned short*) 0xB8000;
  /* clear first lines */
  for (int i = 0; i < 80 * 2; ++i)
    vga[i] = (unsigned short) (' ' | (0x0F << 8));
  int i = 0;
  while (msg[i] && i < 80 * 2 - 1)
  {
    vga[i] = (unsigned short) (msg[i] | (0x0F << 8));
    ++i;
  }

  for (;;)
  {
    asm volatile("hlt");
  }
}
