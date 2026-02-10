#include "serial/serial.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
  uint8_t val;
  asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

void serial_init(void) {
  const uint16_t COM1 = 0x3F8;
  outb(COM1 + 1, 0x00); // disable all interrupts
  outb(COM1 + 3, 0x80); // enable DLAB (set baud rate divisor)
  outb(COM1 + 0, 0x01); // divisor = 1 (lo byte) 115200 baud
  outb(COM1 + 1, 0x00); // divisor hi byte
  outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
  outb(COM1 + 2, 0xC7); // enable FIFO, clear them, with 14-byte threshold
  outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_putc(char c) {
  const uint16_t COM1 = 0x3F8;
  while ((inb(COM1 + 5) & 0x20) == 0) {
  }
  outb(COM1 + 0, (uint8_t)c);
}

void serial_puts(const char *s) {
  while (*s)
    serial_putc(*s++);
}

void serial_puthex64(uint64_t v) {
  const char *hex = "0123456789ABCDEF";
  serial_puts("0x");
  for (int i = 15; i >= 0; --i) {
    serial_putc(hex[(v >> (i * 4)) & 0xF]);
  }
  serial_puts("\n");
}

void serial_putdec(uint64_t v) {
  char buf[32];
  int i = 0;
  if (v == 0) {
    serial_putc('0');
    serial_puts("\n");
    return;
  }
  while (v) {
    buf[i++] = '0' + (v % 10);
    v /= 10;
  }
  for (int j = i - 1; j >= 0; --j)
    serial_putc(buf[j]);
  serial_puts("\n");
}
