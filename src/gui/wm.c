#include "drivers/mouse/mouse.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "gui/mia.h"
#include "multitasking/scheduler.h"
#include "serial/serial.h"
#include <stdint.h>
#include <stdio.h>

static void draw_task_list(MiaWindow *w) {
  if (!w)
    return;
  struct scheduler_task_info buf[16];
  int n = scheduler_get_tasks(buf, 16);
  int wx, wy, ww, wh;
  mia_get_window_rect(w, &wx, &wy, &ww, &wh);
  int gx = wx + 6;
  int gy = wy + 22;
  for (int i = 0; i < n; ++i) {
    char line[64];
    snprintf(line, sizeof(line), "task %d: used=%d dead=%d", buf[i].id,
             buf[i].used, buf[i].dead);
    int y = gy + i * (psf_get_glyph_height() + 2);
    psf_draw_text(gx, y, line, 0xFFFFFF);
  }
}

void wm_proc(void *arg) {
  (void)arg;
  serial_puts("wm: start\n");

  MiaWindow *w = mia_window_create(10, 10, 240, 160, "WM");
  MiaWindow *tasks = mia_window_create(260, 10, 220, 160, "Tasks");

  int dragging = 0;
  MiaWindow *drag_win = NULL;
  int offx = 0, offy = 0;
  int btn_prev = 0;

  uint8_t pkt[3];
  while (1) {
    /* poll mouse (non-blocking) */
    if (mouse_get_packet(pkt)) {
      int left = pkt[0] & 1;
      int dx = (int8_t)pkt[1];
      int dy = (int8_t)pkt[2];
      int cx, cy;
      mia_get_cursor(&cx, &cy);
      cx += dx;
      cy -= dy; /* PS/2 dy is negative up */
      if (cx < 0)
        cx = 0;
      if (cy < 0)
        cy = 0;
      if ((uint64_t)cx >= framebuffer_get_width())
        cx = framebuffer_get_width() - 1;
      if ((uint64_t)cy >= framebuffer_get_height())
        cy = framebuffer_get_height() - 1;
      mia_set_cursor(cx, cy);

      if (left && !btn_prev) {
        /* pressed */
        MiaWindow *hit = mia_window_at(cx, cy);
        if (hit) {
          int hx, hy, hww, hhh;
          mia_get_window_rect(hit, &hx, &hy, &hww, &hhh);
          drag_win = hit;
          offx = cx - hx;
          offy = cy - hy;
          mia_bring_to_front(drag_win);
          dragging = 1;
        }
      } else if (!left && btn_prev) {
        /* released */
        if (dragging) {
          dragging = 0;
          drag_win = NULL;
        }
      }
      btn_prev = left;

      if (dragging && drag_win) {
        int nx = cx - offx;
        int ny = cy - offy;
        mia_window_move(drag_win, nx, ny);
      }
    }

    /* redraw: clear background (reserve bottom strip), paint windows, draw
     * cursor and task list */
    int reserve = psf_get_glyph_height() + 16;
    int clear_h = (int)framebuffer_get_height() > reserve
                      ? (int)framebuffer_get_height() - reserve
                      : (int)framebuffer_get_height();
    framebuffer_draw_rect(0, 0, framebuffer_get_width(), (uint64_t)clear_h,
                          0x000088);
    /* paint windows but clipped to avoid the reserved bottom area */
    mia_paint_clipped(clear_h);
    if (tasks)
      draw_task_list(tasks);
    /* don't draw cursor if it's in the reserved area */
    int cx, cy;
    mia_get_cursor(&cx, &cy);
    if (cy < (int)framebuffer_get_height() - reserve)
      mia_draw_cursor();

    scheduler_yield();
  }
}