#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H
#include <stddef.h>
extern unsigned char heap[];
void *kmalloc(size_t size);
void kfree(void *ptr);
/* Return the usable size for a previously kmalloc'd pointer (0 if NULL) */
size_t kalloc_usable_size(void *ptr);
#define KERNEL_HEAP_START 0xFFFFFFFF801A8000ULL
#define KERNEL_HEAP_SIZE  (8 * 1024 * 1024)
#endif
