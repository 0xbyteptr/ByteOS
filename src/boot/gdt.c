#include "boot/gdt.h"
#include "serial/serial.h"
#include <stdint.h>
#include <stddef.h>

struct __attribute__((packed)) tss_struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
};

static struct tss_struct tss;
extern uint8_t kernel_stack_top[];

static uint64_t gdt[7];
static struct gdt_ptr gp;

static uint64_t gdt_entry(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    uint64_t r = 0;
    r  = (limit & 0xFFFFULL);
    r |= ((uint64_t)(base & 0xFFFFFFULL)) << 16;
    r |= ((uint64_t)access) << 40;
    r |= ((uint64_t)((limit >> 16) & 0xF)) << 48;
    r |= ((uint64_t)(flags & 0xF)) << 52;
    r |= ((uint64_t)((base >> 24) & 0xFF)) << 56;
    return r;
}

void gdt_init(void) {
    serial_puts("gdt: init\n");

    gdt[0] = 0;                              // NULL
        serial_puts("gdt: about to set 1\n");
    gdt[1] = gdt_entry(0, 0, 0x9A, 0xA);    // Kernel code
        serial_puts("gdt: set 1\n");
        serial_puts("gdt: about to set 2\n");
    gdt[2] = gdt_entry(0, 0, 0x92, 0x0);    // Kernel data
        serial_puts("gdt: set 2\n");
        serial_puts("gdt: about to set 3\n");
    gdt[3] = gdt_entry(0, 0, 0xFA, 0xA);    // User code
        serial_puts("gdt: set 3\n");
        serial_puts("gdt: about to set 4\n");
    gdt[4] = gdt_entry(0, 0, 0xF2, 0x0);    // User data
        serial_puts("gdt: set 4\n");
        serial_puts("gdt: about to set 5\n");
    gdt[5] = 0;                              // placeholder TSS low
        serial_puts("gdt: set 5\n");
        serial_puts("gdt: about to set 6\n");
    gdt[6] = 0;                              // placeholder TSS high
        serial_puts("gdt: set 6\n");

    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)&gdt;

    serial_puts("gdt: gp.limit = ");
    serial_putdec(gp.limit);
    serial_puts("\ngdt: gp.base = ");
    serial_puthex64(gp.base);
    serial_puts("\n");

    extern void __load_gdt_asm(struct gdt_ptr *gp);
    __load_gdt_asm(&gp);
    serial_puts("gdt: __load_gdt_asm returned\n");
}

void gdt_set_tss(uint64_t tss_addr, uint32_t tss_limit) {
    serial_puts("gdt_set_tss: tss_addr = ");
    serial_puthex64(tss_addr);
    serial_puts(", tss_limit = ");
    serial_putdec(tss_limit);
    serial_puts("\n");

    uint64_t lo = 0;
    lo  = (tss_limit & 0xFFFFULL);
    lo |= (tss_addr & 0xFFFFFFULL) << 16;
    lo |= ((uint64_t)0x89) << 40;
    lo |= ((tss_limit >> 16) & 0xF) << 48;
    lo |= 0x0ULL << 52;
    lo |= ((tss_addr >> 24) & 0xFF) << 56;

    uint64_t hi = (tss_addr >> 32) & 0xFFFFFFFFULL;

    gdt[5] = lo;
    gdt[6] = hi;

    serial_puts("gdt_set_tss: gdt[5] = ");
    serial_puthex64(gdt[5]);
    serial_puts(", gdt[6] = ");
    serial_puthex64(gdt[6]);
    serial_puts("\n");

    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)&gdt;

    extern void __load_gdt_asm(struct gdt_ptr *gp);
    __load_gdt_asm(&gp);

    serial_puts("gdt_set_tss: calling __load_tr_asm\n");
    extern void __load_tr_asm(uint16_t sel);
    __load_tr_asm(0x28);
    serial_puts("gdt_set_tss: __load_tr_asm returned\n");
}
