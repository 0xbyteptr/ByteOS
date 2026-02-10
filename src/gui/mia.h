#ifndef MIA_H
#define MIA_H

#include <stdint.h>

typedef struct MiaWindow MiaWindow;

void mia_init(void);
MiaWindow *mia_window_create(int x, int y, int w, int h, const char *title);
void mia_window_move(MiaWindow *w, int x, int y);

/* z-order and cursor APIs */
void mia_bring_to_front(MiaWindow *w);
void mia_set_cursor(int x, int y);
void mia_get_cursor(int *x, int *y);
void mia_draw_cursor(void);
MiaWindow *mia_window_at(int x, int y);
void mia_get_window_rect(MiaWindow *w, int *x, int *y, int *w_out, int *h_out);

void mia_paint_all(void);
void mia_paint_clipped(int max_h);
int mia_get_width(MiaWindow *w);
int mia_get_height(MiaWindow *w);

/* debug helper: print window fields to serial */
void mia_dump_window(MiaWindow *w);

#endif
