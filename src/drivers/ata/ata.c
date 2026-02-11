#include "drivers/ata/ata.h"
#include "serial/serial.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port)
{
  uint8_t v;
  asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}
static inline uint16_t inw(uint16_t port)
{
  uint16_t v;
  asm volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD 0x1F7
#define ATA_CTRL 0x3F6

/* wait until not busy */
static int ata_wait_busy(void)
{
  for (int i = 0; i < 1000000; ++i)
  {
    uint8_t s = inb(ATA_STATUS);
    if (!(s & 0x80))
      return 0; /* BSY clear */
  }
  return -1;
}

int ata_init(void)
{
  serial_puts("ata: init - probing primary controller\n");
  /* simple probe: read status */
  uint8_t s = inb(ATA_STATUS);
  if (s == 0xFF)
  {
    serial_puts("ata: controller not present\n");
    return -1;
  }
  serial_puts("ata: controller present\n");
  return 0;
}

/* Read a single sector via PIO (LBA28). Assumes drive/head already selected */
static int ata_read_sector_pio(uint32_t lba, uint8_t* buf)
{
  if (ata_wait_busy() != 0)
    return -1;
  /* select LBA28, master drive (0xE0) | (LBA >> 24 & 0x0F) */
  outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_SECTOR_CNT, 1);
  outb(ATA_LBA_LOW, (uint8_t) (lba & 0xFF));
  outb(ATA_LBA_MID, (uint8_t) ((lba >> 8) & 0xFF));
  outb(ATA_LBA_HIGH, (uint8_t) ((lba >> 16) & 0xFF));
  outb(ATA_CMD, 0x20); /* READ SECTORS */

  /* wait for DRQ */
  for (int i = 0; i < 1000000; ++i)
  {
    uint8_t s = inb(ATA_STATUS);
    if (s & 0x08)
      break; /* DRQ */
    if (s & 0x01)
      return -1; /* ERR */
  }

  /* read 256 words */
  for (int i = 0; i < 256; ++i)
  {
    uint16_t w     = inw(ATA_DATA);
    buf[i * 2]     = (uint8_t) (w & 0xFF);
    buf[i * 2 + 1] = (uint8_t) (w >> 8);
  }
  return 0;
}

int ata_read_lba(uint32_t lba, uint8_t* buf, int sectors)
{
  for (int s = 0; s < sectors; ++s)
  {
    if (ata_read_sector_pio(lba + s, buf + s * 512) != 0)
      return -1;
  }
  return 0;
}
