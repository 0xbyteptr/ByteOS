#ifndef GRAPHICS_FONT_H
#define GRAPHICS_FONT_H

#include <stddef.h>
#include <stdint.h>

/* Try to initialize PSF font from memory; returns 1 on success */
int psf_init_from_memory(const void *data, size_t len);

/* Draw text at pixel position (x,y) in given color (packed as framebuffer expects) */
void psf_draw_text(int x, int y, const char *s, uint32_t color);

/* Font metric helpers */
int psf_get_glyph_width(void);
int psf_get_glyph_height(void);

#endif
