#ifndef FAT_H
#define FAT_H

#include <stddef.h>

int fat_init(void);
int fat_read_file(const char *name, void **out_buf, size_t *out_len);

#endif
