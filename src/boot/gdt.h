#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init(void);
void tss_init(void);
void gdt_set_tss(uint64_t tss_addr, uint32_t tss_limit);

#endif
