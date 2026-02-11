#include "drivers/ext/ext.h"
#include "drivers/ata/ata.h"
#include "lib/libc.h"
#include "serial/serial.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t ext_block_size       = 1024;
static uint32_t ext_inode_size       = 128;
static uint32_t ext_blocks_per_group = 0;
static uint32_t ext_inodes_per_group = 0;
static uint32_t ext_first_data_block = 0;
static uint32_t ext_part_lba         = 0;

static int read_block(uint32_t blk, void* buf)
{
  /* read one block (block size assumed 1024/2048/4096) */
  uint32_t sectors = ext_block_size / 512;
  return ata_read_lba(ext_part_lba + blk * sectors, buf, sectors);
}

int ext_init(void)
{
  serial_puts("ext: init - reading superblock\n");
  uint8_t buf[1024];
  /* superblock located at offset 1024 from start of FS */
  if (ata_read_lba(2, buf, 2) != 0)
  {
    serial_puts("ext: cannot read superblock\n");
    return -1;
  }
  uint32_t s_log_block_size = *(uint32_t*) &buf[24];
  ext_block_size            = 1024u << s_log_block_size;
  ext_inode_size            = *(uint16_t*) &buf[88];
  if (ext_inode_size == 0)
    ext_inode_size = 128;
  ext_inodes_per_group = *(uint32_t*) &buf[40];
  ext_blocks_per_group = *(uint32_t*) &buf[32];
  ext_first_data_block = *(uint32_t*) &buf[20];
  serial_puts("ext: superblock parsed\n");
  return 0;
}

/* very naive read by inode (direct blocks only) */
static int ext_read_inode_data(uint32_t inode, void** out_buf, size_t* out_len)
{
  uint8_t iblock[4096];
  /* locate inode table: assume group 0 and inode table starts at block in group
   * descriptor at block 3 */
  if (read_block(3, iblock) != 0)
    return -1;
  uint32_t inode_table = *(uint32_t*) &iblock[8 * 32 + 8];
  uint32_t inode_index = inode - 1;
  uint32_t inode_block = inode_table + (inode_index * ext_inode_size) / ext_block_size;
  if (read_block(inode_block, iblock) != 0)
    return -1;
  uint8_t*  inode_ptr   = iblock + (inode_index * ext_inode_size) % ext_block_size;
  uint32_t  size_lo     = *(uint32_t*) &inode_ptr[4];
  uint32_t  blocks      = *(uint32_t*) &inode_ptr[28];
  uint32_t* blocks_list = (uint32_t*) &inode_ptr[40];
  void*     buf         = malloc(size_lo);
  if (!buf)
    return -1;
  size_t copied = 0;
  for (int i = 0; i < 12 && copied < size_lo; ++i)
  {
    uint32_t b = blocks_list[i];
    if (b == 0)
      break;
    if (read_block(b, iblock) != 0)
    {
      free(buf);
      return -1;
    }
    size_t tocopy = ext_block_size;
    if (copied + tocopy > size_lo)
      tocopy = size_lo - copied;
    memcpy((uint8_t*) buf + copied, iblock, tocopy);
    copied += tocopy;
  }
  *out_buf = buf;
  *out_len = size_lo;
  return 0;
}

int ext_read_file(const char* path, void** out_buf, size_t* out_len)
{
  /* very naive: only root directory and files directly in it by name */
  (void) path;
  uint8_t buf[4096];
  if (read_block(5, buf) != 0)
    return -1; /* assume root dir is at block 5 */
  /* not implemented fully */
  serial_puts("ext: ext_read_file not implemented fully\n");
  return -1;
}
