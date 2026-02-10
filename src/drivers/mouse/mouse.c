#include "drivers/mouse/mouse.h"
#include "serial/serial.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
  uint8_t v;
  asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}
static inline void outb(uint16_t port, uint8_t v) {
  asm volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

int mouse_init(void) {
  serial_puts("mouse: init - PS/2 mouse polling\n");
  /* Enable mouse in PS/2 controller could be added, but keep simple */
  return 0;
}

int mouse_get_packet(uint8_t out[3]) {
  uint8_t status = inb(0x64);
  if (!(status & 0x01))
    return 0;
  /* Read 3 bytes if available - simplistic */
  out[0] = inb(0x60);
  out[1] = inb(0x60);
  out[2] = inb(0x60);
  return 1;
}
