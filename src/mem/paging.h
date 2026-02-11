#ifndef MEM_PAGING_H
#define MEM_PAGING_H

#include <stddef.h>
#include <stdint.h>

/* Mark pages covering [va, va+size) as user-accessible by setting the U bit
 * on existing page-table entries. Returns 0 on success, -1 on failure. */
int paging_set_user(void *va, size_t size);

/* Map kernel va to user va */
int paging_map_user_va(uint64_t user_va, uint64_t kernel_va, size_t size);

/* Allocate a physical page */
uint64_t alloc_page(void);

/* Get physical address of va */
uint64_t get_phys(uint64_t va);

#endif
