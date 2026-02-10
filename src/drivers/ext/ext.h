#ifndef EXT_H
#define EXT_H

#include <stddef.h>

int ext_init(void);
int ext_read_file(const char *path, void **out_buf, size_t *out_len);

#endif
