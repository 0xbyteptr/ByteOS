#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H
#include <stddef.h>
void *kmalloc(size_t size);
void kfree(void *ptr);
/* Return the usable size for a previously kmalloc'd pointer (0 if NULL) */
size_t kalloc_usable_size(void *ptr);
#endif
