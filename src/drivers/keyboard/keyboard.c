#include "drivers/keyboard/keyboard.h"
#include "serial/serial.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port)
{
  uint8_t v;
  asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}
static inline void outb(uint16_t port, uint8_t v)
{
  asm volatile("outb %0, %1" ::"a"(v), "Nd"(port));
}

#define KB_BUF_SIZE 256
static uint8_t kb_buf[KB_BUF_SIZE];
static int     head = 0, tail = 0;

static int shift = 0;
static int caps  = 0;

/* Scancode set 1 normal / shifted mapping */
static const char scmap[256] = {
    [0x02] = '1',  [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', [0x07] = '6',
    [0x08] = '7',  [0x09] = '8', [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x10] = 'q',  [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't', [0x15] = 'y',
    [0x16] = 'u',  [0x17] = 'i', [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h',  [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
    [0x29] = '`',  [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n',  [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x39] = ' '};

/* Shifted keys */
static const char scmap_shift[256] = {
    [0x02] = '!',  [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^',
    [0x08] = '&',  [0x09] = '*', [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x10] = 'Q',  [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T', [0x15] = 'Y',
    [0x16] = 'U',  [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n', [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H',  [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':', [0x28] = '"',
    [0x29] = '~',  [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N',  [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?', [0x39] = ' '};

int keyboard_init(void)
{
  serial_puts("keyboard: init - polling PS/2\n");
  head = tail = 0;
  shift = caps = 0;
  return 0;
}

void keyboard_poll(void)
{
  uint8_t status = inb(0x64);
  if (!(status & 0x01))
    return;

  uint8_t sc = inb(0x60);

  int released = sc & 0x80;
  sc &= 0x7F;

  /* Handle modifiers */
  if (sc == 0x2A || sc == 0x36)
  { /* LShift / RShift */
    shift = !released;
    return;
  }
  if (sc == 0x3A && !released)
  { /* CapsLock toggle */
    caps = !caps;
    return;
  }

  /* Ignore key release for normal keys */
  if (released)
    return;

  /* Map scancode to char */
  char c = shift ? scmap_shift[sc] : scmap[sc];
  if (!c)
    return;

  /* Apply CapsLock for letters */
  if (caps && c >= 'a' && c <= 'z')
    c -= 32;

  /* Store in circular buffer */
  kb_buf[head] = c;
  head         = (head + 1) % KB_BUF_SIZE;
  if (head == tail)
  { /* buffer full, drop oldest */
    tail = (tail + 1) % KB_BUF_SIZE;
  }
}

int keyboard_getchar(void)
{
  if (head == tail)
    return -1; /* empty */
  char c = kb_buf[tail];
  tail   = (tail + 1) % KB_BUF_SIZE;
  return c;
}
