#include "../assets/bg.h"
#include "../assets/font.h"
#include "../boot/limine.h"
#include "../compat/panic.h"
#include "../graphics/font.h"
#include "../graphics/framebuffer.h"
#include "../lib/libc.h"
#include "../lib/stb_image_stub.h"
#include "../mem/alloc.h"
#include <stddef.h>
#include <stdint.h>

#include "boot/gdt.h"
#include "console/console.h"
#include "drivers/drivers.h"
#include "gui/mia.h"
#include "multitasking/scheduler.h"
#include "serial/serial.h"
#include "shell/shell.h"

extern void console_init(void);
extern void gdt_init(void);
extern void tss_init(void);
extern void early_idt_init(void);
extern void idt_init(void);

static void draw_rect(uint8_t *fb, uint64_t fb_pitch, uint64_t x, uint64_t y,
                      uint64_t w, uint64_t h, uint32_t color, uint16_t bpp) {
  if (!fb || bpp == 0)
    return;

  uint8_t bytes = bpp / 8;
  for (uint64_t yy = 0; yy < h; ++yy) {
    uint8_t *line = fb + (y + yy) * fb_pitch + x * bytes;
    for (uint64_t xx = 0; xx < w; ++xx) {
      uint8_t *p = line + xx * bytes;
      if (bytes >= 4) {
        *((uint32_t *)p) = color;
      } else if (bytes == 3) {
        p[0] = (uint8_t)(color & 0xFF);
        p[1] = (uint8_t)((color >> 8) & 0xFF);
        p[2] = (uint8_t)((color >> 16) & 0xFF);
      }
    }
  }
}

static uint32_t compose_pixel(uint8_t r, uint8_t g, uint8_t b,
                              struct limine_framebuffer *fb) {
  if (!fb)
    return 0;

  uint32_t out = 0;
  uint32_t rv = (uint32_t)r >> (8 - fb->red_mask_size);
  uint32_t gv = (uint32_t)g >> (8 - fb->green_mask_size);
  uint32_t bv = (uint32_t)b >> (8 - fb->blue_mask_size);

  out |= (rv << fb->red_mask_shift);
  out |= (gv << fb->green_mask_shift);
  out |= (bv << fb->blue_mask_shift);

  return out;
}

static void mover(void *arg) {
  serial_puts("mover: start\n");
  MiaWindow *w = mia_window_create(60, 60, 220, 160, "Mover");
  if (!w) {
    serial_puts("mover: mia_window_create returned NULL\n");
    for (;;)
      asm volatile("hlt");
  }

  serial_puts("mover: window created\n");
  int dir = 1;
  int x = 60;

  while (1) {
    serial_puts("mover: tick\n");
    mia_window_move(w, x, 60);

    uint64_t fb_width = framebuffer_get_width();
    uint64_t fb_height = framebuffer_get_height();
    uint64_t clear_height = (fb_height > 48) ? fb_height - 48 : fb_height;

    framebuffer_draw_rect(0, 0, fb_width, clear_height, 0x000088);
    mia_paint_all();

    x += dir * 4;
    if (x < 10 || x + mia_get_width(w) > (int)fb_width - 10) {
      dir = -dir;
    }

    scheduler_yield();
  }
}

static void looper(void *arg) {
  int i = 0;
  while (1) {
    printf("Loop %d\n", i++);
    scheduler_yield();
  }
}

void kernel_main(void) {
  volatile unsigned short *vga = (unsigned short *)0xB8000;
  if (!vga) {
    serial_puts("kernel: VGA memory not accessible\n");
    return;
  }

  vga[0] = (unsigned short)('B' | (0x0F << 8));
  vga[1] = (unsigned short)('O' | (0x0F << 8));
  vga[2] = (unsigned short)('O' | (0x0F << 8));
  vga[3] = (unsigned short)('T' | (0x0F << 8));

  serial_init();
  serial_puts("kernel: enter\n");

  gdt_init();
  serial_puts("kernel: gdt_init returned\n");

  early_idt_init();
  serial_puts("kernel: early_idt_init returned\n");

  tss_init();
  serial_puts("kernel: tss_init returned\n");

  idt_init();
  serial_puts("kernel: idt_init returned\n");

  asm volatile("sti");

  drivers_init_all();

  if (!framebuffer_init()) {
    serial_puts(
        "kernel: framebuffer init failed; continuing without framebuffer\n");
    vga[0] = (unsigned short)('N' | (0x0F << 8));
    vga[1] = (unsigned short)('O' | (0x0F << 8));
    vga[2] = (unsigned short)('F' | (0x0F << 8));
    vga[3] = (unsigned short)('B' | (0x0F << 8));
  } else {
    serial_puts("kernel: framebuffer initialized\n");
    int psf_ok =
        psf_init_from_memory(assets_font_psf1, (size_t)assets_font_psf1_len);
    if (!psf_ok) {
      serial_puts("kernel: psf_init failed\n");
    } else {
      serial_puts("kernel: psf_init ok\n");
      console_init();
    }
  }

  framebuffer_draw_rect(0, 0, framebuffer_get_width(), framebuffer_get_height(),
                        0x1E2A38);
  psf_draw_text(20, 20, "ByteOS", 0xFFFFFF);

  if (scheduler_init() != 0) {
    serial_puts("kernel: scheduler init failed\n");
    panic("Scheduler initialization failed");
  }

  mia_init();

  extern void wm_proc(void *);
  int wm_tid = task_create(wm_proc, NULL);
  if (wm_tid < 0) {
    serial_puts("kernel: failed to create wm task\n");
  }

  extern void user_main(void);
  extern void enter_user_task(void *arg);
  int uid = task_create((task_fn)enter_user_task, (void *)user_main);
  if (uid < 0) {
    serial_puts("kernel: failed to create user task\n");
  }

  int t1 = task_create(mover, NULL);
  if (t1 < 0) {
    serial_puts("kernel: failed to create mover task\n");
  }

  int t2 = task_create(looper, NULL);
  if (t2 < 0) {
    serial_puts("kernel: failed to create looper task\n");
  }

  int ts = task_create(shell_proc, NULL);
  if (ts < 0) {
    serial_puts("kernel: failed to create shell task\n");
  }

  serial_puts("kernel: demo tasks created\n");
  serial_puts("kernel: starting scheduler\n");
  scheduler_run();
}
