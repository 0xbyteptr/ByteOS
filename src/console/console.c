#include "console/console.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "lib/libc.h"
#include "serial/serial.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static int      cursor_x = 0;
static int      cursor_y = 0;
static int      glyph_w  = 8;
static int      glyph_h  = 16;
static uint32_t fg_color = 0xFFFFFF;

void console_init(void)
{
  glyph_w  = psf_get_glyph_width();
  glyph_h  = psf_get_glyph_height();
  cursor_x = 8; /* small margin */
  cursor_y = 8;
}

/* draw a single line at current cursor, advance cursor to next line */
static void console_draw_line(const char* line)
{
  psf_draw_text(cursor_x, cursor_y, line, fg_color);
  cursor_y += glyph_h + 2;
  /* simple scrolling: if we exceed framebuffer, clear screen and reset */
  if ((uint64_t) cursor_y + glyph_h > framebuffer_get_height())
  {
    framebuffer_draw_rect(0, 0, framebuffer_get_width(), framebuffer_get_height(), 0x000000);
    cursor_y = 8;
  }
}

void console_puts(const char* s)
{
  if (!s)
    return;
  char   buf[1024];
  size_t bi = 0;
  while (*s)
  {
    if (*s == '\n' || bi >= sizeof(buf) - 1)
    {
      buf[bi] = '\0';
      console_draw_line(buf);
      bi = 0;
      if (*s == '\n')
      {
        ++s;
        continue;
      }
    }
    else
    {
      buf[bi++] = *s++;
    }
  }
  if (bi)
  {
    buf[bi] = '\0';
    console_draw_line(buf);
  }
}

void console_printf(const char* fmt, ...)
{
  char    buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  (void) n;
  /* also emit to serial for logs */
  console_puts(buf);
}
