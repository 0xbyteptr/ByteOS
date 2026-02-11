#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include <stddef.h>
#include <stdint.h>

/* Minimal PSF1 header */
struct psf1_header
{
  uint8_t magic[2]; /* 0x36 0x04 */
  uint8_t mode;     /* bits: 0 - 256 glyphs */
  uint8_t charsize; /* bytes per glyph */
};

static const uint8_t* glyphs       = NULL;
static unsigned       glyph_bytes  = 0;
static unsigned       glyph_count  = 0;
static unsigned       glyph_height = 0;
static unsigned       glyph_width  = 8; /* PSF1 fonts are typically 8 pixels wide */
static int            psf_loaded   = 0;

int psf_init_from_memory(const void* data, size_t len)
{
  if (!data || len < sizeof(struct psf1_header))
    return 0;
  const struct psf1_header* h = (const struct psf1_header*) data;
  if (h->magic[0] != 0x36 || h->magic[1] != 0x04)
    return 0;
  glyph_bytes = h->charsize;
  glyphs      = (const uint8_t*) data + sizeof(struct psf1_header);
  if ((size_t) glyph_bytes * 256 <= len - sizeof(struct psf1_header))
  {
    glyph_count = 256;
  }
  else
  {
    glyph_count = 512; /* fallback, but likely wrong */
  }
  glyph_height = glyph_bytes;
  psf_loaded   = 1;
  return 1;
}

/* Fallback simple rectangle-box glyph if no real PSF loaded */
static void draw_char_fallback(int px, int py, char c, uint32_t color)
{
  /* each "char" represented by a simple 10x16 filled block with a notch for
   * visual variety */
  int w = 10, h = 16;
  /* draw background darker rectangle first (a border look) */
  framebuffer_draw_rect(px, py, w, h, 0x202020);
  /* draw inner smaller rectangle as "ink" */
  framebuffer_draw_rect(px + 2, py + 3, w - 4, h - 6, color);
}

static void draw_glyph(int px, int py, const uint8_t* g, uint32_t color)
{
  /* g points to glyph_bytes rows of 8-bit columns */
  for (unsigned row = 0; row < glyph_height; ++row)
  {
    uint8_t bits = g[row];
    for (unsigned col = 0; col < glyph_width; ++col)
    {
      if (bits & (1 << (7 - col)))
      {
        framebuffer_draw_rect(px + col, py + row, 1, 1, color);
      }
    }
  }
}

void psf_draw_text(int x, int y, const char* s, uint32_t color)
{
  if (!s)
    return;
  int cx = x;
  while (*s)
  {
    char c = *s++;
    if (c == '\n')
    {
      y += (int) glyph_height + 2;
      cx = x;
      continue;
    }
    if (psf_loaded && (unsigned char) c < glyph_count)
    {
      const uint8_t* g = glyphs + ((unsigned char) c) * glyph_bytes;
      draw_glyph(cx, y, g, color);
      cx += (int) glyph_width + 1;
    }
    else
    {
      draw_char_fallback(cx, y, c, color);
      cx += 12; /* fallback advance */
    }
  }
}

int psf_get_glyph_width(void)
{
  return (int) glyph_width;
}

int psf_get_glyph_height(void)
{
  return (int) glyph_height ? (int) glyph_height : 16; /* fallback height */
}
