#include "drivers/fat/fat.h"
#include "drivers/ata/ata.h"
#include "lib/libc.h"
#include "serial/serial.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t part_lba = 0; /* partition start */
static uint16_t bytes_per_sector = 512;
static uint8_t sectors_per_cluster = 0;
static uint32_t reserved_sectors = 0;
static uint8_t num_fats = 0;
static uint32_t fat_size_sectors = 0;
static uint32_t root_cluster = 0;
static uint32_t first_data_sector = 0;

/* Read a sector into buffer (512 bytes) */
static int read_sector(uint32_t lba, void *buf) {
  return ata_read_lba(part_lba + lba, buf, 1);
}

static int read_sectors(uint32_t lba, void *buf, int cnt) {
  return ata_read_lba(part_lba + lba, buf, cnt);
}

int fat_init(void) {
  serial_puts("fat: init - probing MBR/boot sector\n");
  uint8_t sec[512];
  if (ata_read_lba(0, sec, 1) != 0) {
    serial_puts("fat: unable to read sector 0\n");
    return -1;
  }
  /* Check for MBR signature and find first partition */
  if (sec[510] == 0x55 && sec[511] == 0xAA) {
    /* MBR present; read partition entry at offset 446 */
    uint32_t start = *(uint32_t *)&sec[454];
    if (start == 0)
      start = 0; /* fallback */
    part_lba = start;
  } else {
    part_lba = 0;
  }

  /* read boot sector at partition start */
  if (read_sector(0, sec) != 0) {
    serial_puts("fat: cannot read boot sector\n");
    return -1;
  }

  bytes_per_sector = *(uint16_t *)&sec[11];
  sectors_per_cluster = sec[13];
  reserved_sectors = *(uint16_t *)&sec[14];
  num_fats = sec[16];
  uint16_t root_ent_count = *(uint16_t *)&sec[17];
  uint16_t total_sectors_small = *(uint16_t *)&sec[19];
  uint32_t total_sectors = *(uint32_t *)&sec[32];
  uint16_t fat_size16 = *(uint16_t *)&sec[22];
  uint32_t fat_size32 = *(uint32_t *)&sec[36];

  fat_size_sectors = fat_size16 ? fat_size16 : fat_size32;

  /* FAT32 root cluster location */
  root_cluster = fat_size16 ? 0 : *(uint32_t *)&sec[44];

  uint32_t tot_sectors =
      total_sectors_small ? total_sectors_small : total_sectors;

  first_data_sector = reserved_sectors + (num_fats * fat_size_sectors);

  serial_puts("fat: parsed BPB -- initializing\n");
  return 0;
}

/* convert cluster to sector relative to partition */
static uint32_t cluster_to_sector(uint32_t cluster) {
  return first_data_sector + (cluster - 2) * sectors_per_cluster;
}

/* read cluster into buffer (cluster size = sectors_per_cluster * 512) */
static int read_cluster(uint32_t cluster, void *buf) {
  uint32_t sector = cluster_to_sector(cluster);
  return read_sectors(sector, buf, sectors_per_cluster);
}

/* simple 8.3 comparison helper */
static void name_to_83(const char *name, char out[11]) {
  /* fill with spaces */
  for (int i = 0; i < 11; ++i)
    out[i] = ' ';
  /* parse name.part */
  int ni = 0;
  int pi = 0;
  const char *p = name;
  while (*p && *p != '.') {
    char c = *p++;
    if (c >= 'a' && c <= 'z')
      c -= 32;
    if (ni < 8)
      out[ni++] = c;
  }
  if (*p == '.') {
    ++p;
  }
  while (*p && pi < 3) {
    char c = *p++;
    if (c >= 'a' && c <= 'z')
      c -= 32;
    out[8 + pi++] = c;
  }
}

int fat_read_file(const char *name, void **out_buf, size_t *out_len) {
  if (!name || !out_buf || !out_len)
    return -1;
  uint8_t clbuf[4096]; /* allow for up to 8 sectors */
  uint8_t secbuf[512];
  /* start at root cluster */
  uint32_t cluster = root_cluster ? root_cluster : 2;
  size_t file_size = 0;
  uint32_t first_cluster = 0;

  char want[11];
  name_to_83(name, want);

  while (1) {
    if (read_cluster(cluster, clbuf) != 0)
      return -1;
    /* iterate directory entries */
    for (int off = 0; off < (int)(sectors_per_cluster * 512); off += 32) {
      uint8_t attr = clbuf[off + 11];
      uint8_t first = clbuf[off];
      if (first == 0x00)
        return -1; /* end of dir */
      if (first == 0xE5)
        continue; /* deleted */
      if (attr == 0x0F)
        continue; /* LFN */
      /* compare name */
      if (memcmp(&clbuf[off], want, 11) == 0) {
        first_cluster =
            ((uint32_t)clbuf[off + 26]) | ((uint32_t)clbuf[off + 27] << 8);
        if (fat_size_sectors > 0 && *((uint32_t *)&clbuf[off + 20]) != 0) {
          /* FAT32 high word */
          uint32_t hi = *((uint32_t *)&clbuf[off + 20]);
          first_cluster |= (hi & 0xFFFF) << 16;
        }
        file_size = *((uint32_t *)&clbuf[off + 28]);
        goto found;
      }
    }
    /* TODO: follow cluster chain - naive: assume single cluster dir */
    break;
  }

found:
  if (first_cluster == 0)
    return -1;
  /* allocate buffer and read clusters */
  size_t need = (file_size + bytes_per_sector - 1) & ~(bytes_per_sector - 1);
  void *buf = malloc(need);
  if (!buf)
    return -1;
  size_t read = 0;
  uint32_t cur = first_cluster;
  while (read < file_size) {
    if (read_cluster(cur, clbuf) != 0) {
      free(buf);
      return -1;
    }
    size_t tocopy = sectors_per_cluster * bytes_per_sector;
    if (read + tocopy > file_size)
      tocopy = file_size - read;
    memcpy((uint8_t *)buf + read, clbuf, tocopy);
    read += tocopy;
    /* naive: increment cluster (no FAT traversal) */
    ++cur;
    if (cur == 0)
      break;
  }

  *out_buf = buf;
  *out_len = file_size;
  return 0;
}
