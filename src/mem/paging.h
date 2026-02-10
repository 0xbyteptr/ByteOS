#ifndef MEM_PAGING_H
#define MEM_PAGING_H

#include <stddef.h>

/* Mark pages covering [va, va+size) as user-accessible by setting the U bit
 * on existing page-table entries. Returns 0 on success, -1 on failure. */
int paging_set_user(void *va, size_t size);

#endif
