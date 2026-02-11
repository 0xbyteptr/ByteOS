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

// ────────────────────────────────────────────────
// Forward declarations
// ────────────────────────────────────────────────
extern void console_init(void);
extern void gdt_init(void);
extern void tss_init(void);
extern void early_idt_init(void);
extern void idt_init(void);

extern int kernel_spawn_elf_from_path(const char *path);

// ────────────────────────────────────────────────
// Helper: draw filled rectangle
// ────────────────────────────────────────────────
static void draw_rect(uint8_t *fb, uint64_t fb_pitch, uint64_t x, uint64_t y,
                      uint64_t w, uint64_t h, uint32_t color, uint16_t bpp) {
    if (!fb || bpp == 0 || w == 0 || h == 0) return;

    uint8_t bytes = bpp / 8;
    for (uint64_t yy = 0; yy < h; ++yy) {
        uint8_t *line = fb + (y + yy) * fb_pitch + x * bytes;
        for (uint64_t xx = 0; xx < w; ++xx) {
            uint8_t *p = line + xx * bytes;
            if (bytes >= 4) {
                *(uint32_t *)p = color;
            } else if (bytes == 3) {
                p[0] = (uint8_t)(color);
                p[1] = (uint8_t)(color >> 8);
                p[2] = (uint8_t)(color >> 16);
            }
        }
    }
}

// ────────────────────────────────────────────────
// Simple debug task – prints to serial + tries console
// ────────────────────────────────────────────────
static void debug_console_task(void *arg) {
    (void)arg;
    serial_puts("debug_console_task: started\n");

    for (int i = 0; i < 12; i++) {
        serial_puts("debug_task: count ");
        serial_putdec(i);
        serial_puts(" – ");

        // Try to also print to graphical console
        console_puts("debug ");
        serial_putdec(i);
        console_puts("\n");

        for (volatile int j = 0; j < 20000000; j++); // rough delay
        scheduler_yield();
    }

    serial_puts("debug_console_task: finished\n");
}

// ────────────────────────────────────────────────
// Moving window demo task
// ────────────────────────────────────────────────
static void mover(void *arg) {
    serial_puts("mover task: starting\n");

    MiaWindow *w = mia_window_create(80, 80, 240, 180, "Mover");
    if (!w) {
        serial_puts("mover: failed to create window\n");
        for (;;)
            asm volatile("hlt");
    }

    serial_puts("mover: window created successfully\n");

    int dir = 1;
    int x = 80;

    while (1) {
        mia_window_move(w, x, 80);

        uint64_t wdt = framebuffer_get_width();
        uint64_t hgt = framebuffer_get_height();

        // Clear area below title bar
        uint64_t clear_h = (hgt > 48) ? hgt - 48 : hgt;
        framebuffer_draw_rect(0, 48, wdt, clear_h, 0x000088);

        mia_paint_all();

        x += dir * 5;
        if (x < 20 || x + mia_get_width(w) > (int)wdt - 20) {
            dir = -dir;
        }

        scheduler_yield();
    }
}

// ────────────────────────────────────────────────
// Kernel entry point
// ────────────────────────────────────────────────
void kernel_main(void) {
    // Very early visual feedback (VGA text mode)
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    vga[0] = 'B' | (0x0F << 8);
    vga[1] = 'Y' | (0x0F << 8);
    vga[2] = 'T' | (0x0F << 8);
    vga[3] = 'E' | (0x0F << 8);

    serial_init();
    serial_puts("\n=== ByteOS kernel starting ===\n");

    gdt_init();
    serial_puts("GDT initialized\n");

    early_idt_init();
    serial_puts("Early IDT initialized\n");

    tss_init();
    serial_puts("TSS initialized\n");

    idt_init();
    serial_puts("Full IDT initialized\n");

    asm volatile("sti");    // enable interrupts
    serial_puts("Interrupts enabled\n");

    drivers_init_all();
    serial_puts("Drivers initialized\n");

    // ── Framebuffer ────────────────────────────────
    bool fb_ok = framebuffer_init();
    if (!fb_ok) {
        serial_puts("WARNING: framebuffer_init failed – no graphical output\n");
        vga[5] = 'N' | (0x0C << 8);
        vga[6] = 'O' | (0x0C << 8);
        vga[7] = 'F' | (0x0C << 8);
        vga[8] = 'B' | (0x0C << 8);
    } else {
        serial_puts("Framebuffer initialized (");
        serial_putdec(framebuffer_get_width());
        serial_puts("x");
        serial_putdec(framebuffer_get_height());
        serial_puts(")\n");

        bool font_ok = psf_init_from_memory(assets_font_psf1, assets_font_psf1_len);
        if (font_ok) {
            serial_puts("PSF font loaded\n");
            console_init();
            serial_puts("Console subsystem ready\n");
        } else {
            serial_puts("PSF font loading failed\n");
        }
    }

    // Clear screen / draw background
    if (fb_ok) {
        framebuffer_draw_rect(0, 0,
                              framebuffer_get_width(),
                              framebuffer_get_height(),
                              0x1E2A38);
        psf_draw_text(24, 24, "ByteOS", 0xFFFFFF);
    }

    // ── Scheduler ──────────────────────────────────
    if (scheduler_init() != 0) {
        serial_puts("CRITICAL: scheduler_init failed\n");
        panic("Cannot continue without scheduler");
    }
    serial_puts("Scheduler initialized\n");

    // ── GUI (Mia) ──────────────────────────────────
    mia_init();
    serial_puts("Mia GUI initialized\n");

    // ── Try to start shell from embedded toybox ────
    serial_puts("Trying to start /bin/sh (embedded toybox)...\n");
    int shell_tid = kernel_spawn_elf_from_path("/bin/sh");
    if (shell_tid >= 0) {
        serial_puts("Shell task created successfully – TID = ");
        serial_putdec((uint64_t)shell_tid);
        serial_puts("\n");
    } else {
        serial_puts("!!! Failed to spawn shell – error code = ");
        serial_putdec((uint64_t)shell_tid);
        serial_puts(" !!!\n");
    }

    // ── Debug / demo tasks ─────────────────────────
    int debug_tid = task_create(debug_console_task, NULL);
    if (debug_tid >= 0) {
        serial_puts("Debug console task created – TID = ");
        serial_putdec((uint64_t)debug_tid);
        serial_puts("\n");
    }

    int mover_tid = task_create(mover, NULL);
    if (mover_tid >= 0) {
        serial_puts("Mover window task created – TID = ");
        serial_putdec((uint64_t)mover_tid);
        serial_puts("\n");
    }

    serial_puts("All initial tasks created – entering scheduler\n");
    serial_puts("===========================================\n");

    // Hand over control to the scheduler
    scheduler_run();

    // Should never reach here
    panic("scheduler_run() returned");
}