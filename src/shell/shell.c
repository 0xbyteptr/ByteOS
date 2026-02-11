#include "shell/shell.h"
#include "compat/panic.h"
#include "console/console.h"
#include "drivers/keyboard/keyboard.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "multitasking/scheduler.h"
#include "serial/serial.h"
#include <stdint.h>
#include <string.h>

/* declare kernel helper to spawn ELF by path */
extern int kernel_spawn_elf_from_path(const char *path);

void shell_proc(void *arg) {
  (void)arg;
  serial_puts("shell: start\n");
  console_puts("Welcome to ByteOS shell\n");

  int glyph_w = psf_get_glyph_width();
  int glyph_h = psf_get_glyph_height();
  int fb_w = framebuffer_get_width();
  int fb_h = framebuffer_get_height();
  int reserve = glyph_h + 16;
  int prompt_y = fb_h - glyph_h - 8;

  char line[128];
  int pos = 0;

  while (1) {
    // Clear input area
    framebuffer_draw_rect(0, fb_h > reserve ? fb_h - reserve : 0, fb_w, reserve,
                          0x000000);
    // Draw prompt and buffer
    psf_draw_text(8, prompt_y, "sh> ", 0xFFFFFF);
    if (pos > 0) {
      line[pos] = '\0';
      psf_draw_text(8 + (4 * (glyph_w + 1)), prompt_y, line, 0xFFFFFF);
    }

    int c = keyboard_getchar();

    if (c == -1) {
      // Slight delay and yield
      scheduler_yield();
      continue;
    }

    // Echo key to serial for debugging
    char kb[4] = "";
    kb[0] = (char)c;
    kb[1] = '\0';
    serial_puts("shell:key=");
    serial_puts(kb);
    serial_puts("\n");

    if (c == '\r' || c == '\n') {
      // Execute line
      line[pos] = '\0';
      console_puts("\n");

      if (pos == 0) {
        pos = 0;
        continue;
      }

      if (strcmp(line, "help") == 0) {
        console_puts("commands: help echo ps clear exit panic\n");
      } else if (strncmp(line, "echo ", 5) == 0) {
        console_puts(line + 5);
        console_puts("\n");
      } else if (strcmp(line, "ps") == 0) {
        struct scheduler_task_info tasks[16];
        int n = scheduler_get_tasks(tasks, 16);
        for (int i = 0; i < n; ++i) {
          console_printf("pid=%d used=%d dead=%d\n", tasks[i].id, tasks[i].used,
                         tasks[i].dead);
        }
      } else if (strcmp(line, "clear") == 0) {
        framebuffer_draw_rect(0, 0, fb_w, fb_h, 0x000000);
        console_init();
        } else if (strncmp(line, "exec ", 5) == 0) {
          const char *path = line + 5;
          console_printf("exec: %s\n", path);
          int tid = kernel_spawn_elf_from_path(path);
          if (tid < 0) console_puts("exec: failed to spawn\n");
          else console_printf("exec: spawned pid=%d\n", tid);
      } else if (strcmp(line, "exit") == 0) {
        console_puts("shell: exiting\n");
        return; // Task exits cleanly
      } else if (strcmp(line, "panic") == 0) {
        panic("shell requested panic");
      } else {
        console_puts("unknown command\n");
      }

      pos = 0;
    } else if (c == 8 || c == 127) {
      if (pos > 0) {
        --pos;
      }
    } else if (c >= 32 && c < 127) {
      if (pos < (int)sizeof(line) - 1) {
        line[pos++] = (char)c;
      }
    }
  }
}
