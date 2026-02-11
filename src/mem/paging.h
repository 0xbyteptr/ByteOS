#ifndef MEM_PAGING_H
#define MEM_PAGING_H

#include <stddef.h>
#include <stdint.h>

void paging_init(void);
int paging_set_user(void *va, size_t size);
uint64_t paging_get_phys(uint64_t va);
uint64_t alloc_page(void);
int paging_map_user_va(uint64_t user_va, uint64_t kernel_va, size_t size);
void paging_map_kernel_va(uint64_t kernel_va, size_t size);
void paging_identity_map_kernel_heap(void);
void paging_identity_map_kernel_sections(void);

#endif
