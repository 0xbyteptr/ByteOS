#include "serial/serial.h"
#include <stdint.h>

struct idt_entry {
  uint16_t offset_low;
  uint16_t sel;
  uint8_t ist;
  uint8_t flags;
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t zero;
};
struct __attribute__((packed)) idt_ptr {
  uint16_t limit;
  uint64_t base;
};

static struct idt_entry idt[256] __attribute__((aligned(4096)));
static struct idt_ptr idtp;

extern void __load_idt_asm(struct idt_ptr *p);
extern void __isr_stub_80(void);
extern void __isr_stub_14(void);
extern void __isr_panic(void);

static void set_idt_entry(int n, void *handler, uint16_t sel, uint8_t flags,
                          uint8_t ist) {
  uint64_t addr = (uint64_t)(uintptr_t)handler;
  idt[n].offset_low = addr & 0xFFFF;
  idt[n].sel = sel;
  idt[n].ist = ist & 0x7;
  idt[n].flags = flags;
  idt[n].offset_mid = (addr >> 16) & 0xFFFF;
  idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
  idt[n].zero = 0;
}

/* Install a minimal early IDT that routes all vectors to a panic/halt handler.
   This protects us from stray interrupts while we set up TSS and the final IDT.
 */
void early_idt_init(void) {
  serial_puts("idt: early_init\n");
  serial_puts("idt: filling entries\n");
  for (int i = 0; i < 256; ++i) {
    set_idt_entry(i, __isr_panic, 0x08, 0x8E,
                  0); /* present, DPL=0, interrupt gate */
    if ((i & 63) == 0) {
      serial_puts("idt: set index ");
      serial_putdec((uint64_t)i);
      serial_puts("\n");
    }
  }
  serial_puts("idt: finished filling entries\n");
  idtp.limit = sizeof(idt) - 1;
  idtp.base = (uint64_t)(uintptr_t)&idt;
  serial_puts("idt: idtp.limit = ");
  serial_putdec((uint64_t)idtp.limit);
  serial_puts("idt: idtp.base = ");
  serial_puthex64((uint64_t)idtp.base);
  __load_idt_asm(&idtp);
  serial_puts("idt: early_init returned\n");
}

void idt_init(void) {
  serial_puts("idt: init\n");
  serial_puts("idt: filling entries\n");
  for (int i = 0; i < 256; ++i) {
    set_idt_entry(i, __isr_panic, 0x08, 0x8E, 0);
    if ((i & 63) == 0) {
      serial_puts("idt: set index ");
      serial_putdec((uint64_t)i);
      serial_puts("\n");
    }
  }
  serial_puts("idt: finished filling entries\n");
  /* install int 0x80 with DPL=3 (0xEE flags = present, DPL=3, type=0xE gate) */
  set_idt_entry(0x80, __isr_stub_80, 0x08, 0xEE, 0);
  /* install page fault handler */
  set_idt_entry(14, __isr_stub_14, 0x08, 0x8E, 0);
  idtp.limit = sizeof(idt) - 1;
  idtp.base = (uint64_t)(uintptr_t)&idt;
  serial_puts("idt: idtp.limit = ");
  serial_putdec((uint64_t)idtp.limit);
  serial_puts("idt: idtp.base = ");
  serial_puthex64((uint64_t)idtp.base);
  __load_idt_asm(&idtp);
  serial_puts("idt: init returned\n");
}
