#include "gui/mia.h"
#include "console/console.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "lib/libc.h"
#include "multitasking/scheduler.h"
#include "serial/serial.h"
#include "string.h"

#define MAX_WINDOWS 16

struct MiaWindow
{
  int      used;
  int      x, y, w, h;
  char     title[64];
  uint32_t color_bg;
  int      z;
};

static struct MiaWindow windows[MAX_WINDOWS];
static int              next_z   = 1;
static int              cursor_x = 0, cursor_y = 0; /* cursor position */
static int              cursor_size = 8;

void mia_init(void)
{
  serial_puts("mia: init start\n");
  scheduler_lock();
  mia_paint_all();
  scheduler_unlock();

  /* Clear all window slots (use safe byte-wise volatile writes to avoid any
   * vector/store issues) */
  for (int i = 0; i < MAX_WINDOWS; ++i)
  {
    volatile uint8_t* p = (volatile uint8_t*) &windows[i];
    for (size_t j = 0; j < sizeof(windows[i]); ++j)
      p[j] = 0;
    windows[i].z = 0;
  }

  cursor_x = (int) (framebuffer_get_width() / 2);
  cursor_y = (int) (framebuffer_get_height() / 2);

  serial_puts("mia: init completed\n");
  console_printf("MiaUI initialized\n");
}

// After mia_init, try creating a window:
MiaWindow* mia_window_create(int x, int y, int w, int h, const char* title)
{
  if (w <= 0 || h <= 0)
    return NULL;

  for (int i = 0; i < MAX_WINDOWS; ++i)
  {
    if (!windows[i].used)
    {
      /* volatile write to avoid any surprises */
      volatile int* pused = &windows[i].used;
      *pused              = 1;

      windows[i].x        = x;
      windows[i].y        = y;
      windows[i].w        = w;
      windows[i].h        = h;
      windows[i].color_bg = 0x203050 + (i * 0x001010);
      /* safe copy */
      const char* src = title ? title : "Window";
      for (size_t ci = 0; ci < sizeof(windows[i].title) - 1; ++ci)
      {
        if (!src[ci])
        {
          windows[i].title[ci] = '\0';
          break;
        }
        windows[i].title[ci] = src[ci];
        if (ci == sizeof(windows[i].title) - 2)
          windows[i].title[ci + 1] = '\0';
      }
      windows[i].z = next_z++;
      return &windows[i];
    }
  }
  return NULL;
}

int mia_get_width(MiaWindow* w)
{
  return w ? w->w : 0;
}

int mia_get_height(MiaWindow* w)
{
  return w ? w->h : 0;
}

void mia_window_move(MiaWindow* w, int x, int y)
{
  if (!w)
    return;
  w->x = x;
  w->y = y;
}

static void draw_window(MiaWindow* w)
{
  if (!w)
    return;

  /* title bar */
  framebuffer_draw_rect(w->x, w->y, w->w, 18, 0x101030);
  /* title text */
  psf_draw_text(w->x + 6, w->y + 2, w->title, 0xFFFFFF);
  /* client area */
  framebuffer_draw_rect(w->x, w->y + 18, w->w, w->h - 18, w->color_bg);
  /* border */
  framebuffer_draw_rect(w->x, w->y, w->w, 1, 0x000000);
  framebuffer_draw_rect(w->x, w->y + w->h - 1, w->w, 1, 0x000000);
  framebuffer_draw_rect(w->x, w->y, 1, w->h, 0x000000);
  framebuffer_draw_rect(w->x + w->w - 1, w->y, 1, w->h, 0x000000);
}

void mia_paint_all(void)
{
  /* Paint windows in z-order (lowest z first).  z==0 means unused. */
  int max_z = 0;
  for (int i = 0; i < MAX_WINDOWS; ++i)
    if (windows[i].used && windows[i].z > max_z)
      max_z = windows[i].z;
  for (int z = 1; z <= max_z; ++z)
  {
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
      if (windows[i].used && windows[i].z == z)
        draw_window(&windows[i]);
    }
  }
}

/* Paint windows but clip any parts below 'max_h' so a bottom reserved area
   (e.g. for the shell prompt) is preserved. */
void mia_paint_clipped(int max_h)
{
  int max_z = 0;
  for (int i = 0; i < MAX_WINDOWS; ++i)
    if (windows[i].used && windows[i].z > max_z)
      max_z = windows[i].z;
  for (int z = 1; z <= max_z; ++z)
  {
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
      if (!windows[i].used || windows[i].z != z)
        continue;
      int wy = windows[i].y;
      if (wy >= max_h)
        continue; /* fully in reserved area */
      int draw_h = windows[i].h;
      if (windows[i].y + draw_h > max_h)
        draw_h = max_h - windows[i].y;

      /* title bar */
      framebuffer_draw_rect(windows[i].x, windows[i].y, windows[i].w, 18, 0x101030);
      /* title text */
      psf_draw_text(windows[i].x + 6, windows[i].y + 2, windows[i].title, 0xFFFFFF);
      /* client area clipped */
      framebuffer_draw_rect(windows[i].x,
                            windows[i].y + 18,
                            windows[i].w,
                            draw_h > 18 ? draw_h - 18 : 0,
                            windows[i].color_bg);
      /* border */
      framebuffer_draw_rect(windows[i].x, windows[i].y, windows[i].w, 1, 0x000000);
      framebuffer_draw_rect(windows[i].x, windows[i].y + draw_h - 1, windows[i].w, 1, 0x000000);
      framebuffer_draw_rect(windows[i].x, windows[i].y, 1, draw_h, 0x000000);
      framebuffer_draw_rect(windows[i].x + windows[i].w - 1, windows[i].y, 1, draw_h, 0x000000);
    }
  }
}

void mia_draw_cursor(void)
{
  int cx = cursor_x;
  int cy = cursor_y;
  int s  = cursor_size;
  /* draw a white square with black border */
  framebuffer_draw_rect(cx, cy, s, s, 0xFFFFFF);
  framebuffer_draw_rect(cx, cy, s, 1, 0x000000);
  framebuffer_draw_rect(cx, cy + s - 1, s, 1, 0x000000);
  framebuffer_draw_rect(cx, cy, 1, s, 0x000000);
  framebuffer_draw_rect(cx + s - 1, cy, 1, s, 0x000000);
}

void mia_dump_window(MiaWindow* w)
{
  if (!w)
  {
    serial_puts("mia_dump_window: NULL\n");
    return;
  }
  serial_puts("mia_dump_window: w addr = ");
  serial_puthex64((uint64_t) (uintptr_t) w);
  serial_puts(" x=");
  serial_putdec((uint64_t) w->x);
  serial_puts(" y=");
  serial_putdec((uint64_t) w->y);
  serial_puts(" w=");
  serial_putdec((uint64_t) w->w);
  serial_puts(" h=");
  serial_putdec((uint64_t) w->h);
  serial_puts(" used=");
  serial_putdec((uint64_t) w->used);
  serial_puts(" title=");
  serial_puts(w->title);
  serial_puts(" z=");
  serial_putdec((uint64_t) w->z);
  serial_puts("\n");
}

MiaWindow* mia_window_at(int x, int y)
{
  /* find topmost window at coordinates (x,y) by highest z */
  MiaWindow* best   = NULL;
  int        best_z = 0;
  for (int i = 0; i < MAX_WINDOWS; ++i)
  {
    if (!windows[i].used)
      continue;
    if (x >= windows[i].x && x < windows[i].x + windows[i].w && y >= windows[i].y &&
        y < windows[i].y + windows[i].h)
    {
      if (windows[i].z >= best_z)
      {
        best   = &windows[i];
        best_z = windows[i].z;
      }
    }
  }
  return best;
}

void mia_bring_to_front(MiaWindow* w)
{
  if (!w)
    return;
  w->z = next_z++;
}

void mia_get_window_rect(MiaWindow* w, int* x, int* y, int* w_out, int* h_out)
{
  if (!w)
    return;
  if (x)
    *x = w->x;
  if (y)
    *y = w->y;
  if (w_out)
    *w_out = w->w;
  if (h_out)
    *h_out = w->h;
}

void mia_set_cursor(int x, int y)
{
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if ((uint64_t) x >= framebuffer_get_width())
    x = framebuffer_get_width() - 1;
  if ((uint64_t) y >= framebuffer_get_height())
    y = framebuffer_get_height() - 1;
  cursor_x = x;
  cursor_y = y;
}

void mia_get_cursor(int* x, int* y)
{
  if (x)
    *x = cursor_x;
  if (y)
    *y = cursor_y;
}
