#ifndef ATA_H
#define ATA_H

#include <stdint.h>

int ata_init(void);
/* Read `sectors` starting at `lba` into `buf` (must be at least sectors*512 bytes). Returns 0 on success, -1 on failure. */
int ata_read_lba(uint32_t lba, uint8_t *buf, int sectors);

#endif
