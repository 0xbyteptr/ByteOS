#ifndef GRAPHICS_FRAMEBUFFER_H
#define GRAPHICS_FRAMEBUFFER_H

#include <stdint.h>

/* Initialize framebuffer from Limine request; returns 0 on failure */
int framebuffer_init(void);

/* Blit an RGBA image (4 channels) and center it on the screen */
void framebuffer_blit_rgba(const unsigned char *img, int img_w, int img_h);

/* Draw a filled rectangle */
void framebuffer_draw_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);

/* Basic test fill (small rectangle) */
void framebuffer_test_rect(void);

/* Get current framebuffer dimensions */
uint64_t framebuffer_get_width(void);
uint64_t framebuffer_get_height(void);

#endif
