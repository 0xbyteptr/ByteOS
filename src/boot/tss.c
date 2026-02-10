#include "boot/gdt.h"
#include "serial/serial.h"
#include <stddef.h> // dla size_t
#include <stdint.h>

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
extern uint8_t kernel_stack_top[]; /* defined in entry.S */

void tss_init(void) {
  serial_puts("tss: init\n");
  serial_puts("tss: before zero mem test\n");
  ((uint8_t *)&tss)[0] = 1;
  serial_puts("tss: wrote test byte\n");
  ((uint8_t *)&tss)[0] = 0;
  serial_puts("tss: cleared test byte\n");

  for (size_t i = 0; i < sizeof(tss); ++i) {
    ((volatile uint8_t *)&tss)[i] = 0;
  }
  serial_puts("tss: zeroed\n");

  extern uint8_t kernel_stack_top[];
  serial_puts("tss: setting rsp0\n");
  tss.rsp0 = (uint64_t)(uintptr_t)kernel_stack_top;
  serial_puts("tss: rsp0 set\n");

  tss.io_map_base = sizeof(tss);
  serial_puts("tss: io_map_base set\n");

  extern void gdt_set_tss(uint64_t tss_addr, uint32_t tss_limit);
  serial_puts("tss: calling gdt_set_tss\n");
  gdt_set_tss((uint64_t)(uintptr_t)&tss, sizeof(tss) - 1);
  serial_puts("tss: gdt_set_tss returned\n");
}
