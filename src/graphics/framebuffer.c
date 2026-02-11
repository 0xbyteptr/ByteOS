#include "graphics/framebuffer.h"
#include "boot/limine.h"
#include <stdint.h>

/* ------------------ FRAMEBUFFER ------------------ */

static struct limine_framebuffer* fb        = NULL;
static uint8_t*                   fb_addr   = NULL;
static uint64_t                   fb_width  = 0;
static uint64_t                   fb_height = 0;
static uint64_t                   fb_pitch  = 0;
static uint64_t                   fb_bpp    = 0;

#include "serial/serial.h"

/* ------------------ FRAMEBUFFER INIT ------------------ */
int framebuffer_init(void)
{
  struct limine_framebuffer_response* resp = NULL;

  /* Debug: print the request id words and current response pointer */
  serial_puts("fb: req id = \n");
  serial_puthex64(framebuffer_request.id[0]);
  serial_puthex64(framebuffer_request.id[1]);
  serial_puthex64(framebuffer_request.id[2]);
  serial_puthex64(framebuffer_request.id[3]);
  serial_puts("fb: resp ptr = \n");
  serial_puthex64((uint64_t) framebuffer_request.response);

  /* More debug: dump limine_base_revision address and contents and dump a
     few uint64 words from the .limine_reqs section so we can see exact layout.
   */
  serial_puts("fb: limine_base_revision addr = \n");
  serial_puthex64((uint64_t) (uintptr_t) limine_base_revision);
  serial_puts("fb: limine_base_revision words:\n");
  serial_puthex64((uint64_t) limine_base_revision[0]);
  serial_puthex64((uint64_t) limine_base_revision[1]);
  serial_puthex64((uint64_t) limine_base_revision[2]);

  serial_puts("fb: framebuffer_request addr = \n");
  serial_puthex64((uint64_t) (uintptr_t) &framebuffer_request);

  serial_puts("fb: .limine_reqs dump:\n");
  {
    uint64_t* p = (uint64_t*) (uintptr_t) limine_base_revision;
    for (int k = 0; k < 12; ++k)
      serial_puthex64((uint64_t) p[k]);
  }

  unsigned long i;
  for (i = 0; i < 100000; ++i)
  {
    resp = framebuffer_request.response;
    if (resp && resp->framebuffer_count > 0)
      break;
    if ((i % 20000) == 0)
      serial_puts("fb: waiting...\n");
    asm volatile("hlt");
  }

  if (!resp)
  {
    serial_puts("fb: no response (timed out)\n");
    return 0;
  }
  if (resp->framebuffer_count < 1)
  {
    serial_puts("fb: count=0 (timed out)\n");
    return 0;
  }

  fb = resp->framebuffers[0];
  if (!fb || fb->address == 0)
  {
    serial_puts("fb: invalid framebuffer\n");
    return 0;
  }

  fb_addr   = (uint8_t*) (uintptr_t) fb->address;
  fb_width  = fb->width;
  fb_height = fb->height;
  fb_pitch  = fb->pitch;
  fb_bpp    = fb->bpp;

  serial_puts("fb: init ok\n");
  framebuffer_test_rect();

  return 1;
}

/* ------------------ PIXEL COMPOSITION ------------------ */
static inline uint32_t compose_pixel(uint8_t r, uint8_t g, uint8_t b)
{
  if (fb && (fb->red_mask_size || fb->green_mask_size || fb->blue_mask_size))
  {
    uint32_t r_bits = (uint32_t) r >> (8 - fb->red_mask_size);
    uint32_t g_bits = (uint32_t) g >> (8 - fb->green_mask_size);
    uint32_t b_bits = (uint32_t) b >> (8 - fb->blue_mask_size);
    return (r_bits << fb->red_mask_shift) | (g_bits << fb->green_mask_shift) |
           (b_bits << fb->blue_mask_shift);
  }
  return (r << 16) | (g << 8) | b;
}

/* ------------------ DRAW RECT ------------------ */
void framebuffer_draw_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color)
{
  if (!fb_addr)
    return;

  uint64_t bpp_bytes = fb_bpp / 8;

  for (uint64_t yy = y; yy < y + h && yy < fb_height; ++yy)
  {
    uint8_t* row = fb_addr + yy * fb_pitch;
    for (uint64_t xx = x; xx < x + w && xx < fb_width; ++xx)
    {
      uint8_t* pixel = row + xx * bpp_bytes;
      if (fb_bpp == 32)
        *(uint32_t*) pixel = color;
      else if (fb_bpp == 24)
      {
        pixel[0] = color & 0xFF;
        pixel[1] = (color >> 8) & 0xFF;
        pixel[2] = (color >> 16) & 0xFF;
      }
    }
  }
}

/* ------------------ TEST RECT ------------------ */
void framebuffer_test_rect(void)
{
  uint32_t col = compose_pixel(255, 0, 255);  // magenta
  framebuffer_draw_rect(10, 10, 80, 40, col);
}

/* ------------------ GETTERS ------------------ */
uint64_t framebuffer_get_width(void)
{
  return fb_width;
}
uint64_t framebuffer_get_height(void)
{
  return fb_height;
}

/* ------------------ BLIT RGBA IMAGE ------------------ */
void framebuffer_blit_rgba(const unsigned char* img, int img_w, int img_h)
{
  if (!fb_addr || !img)
    return;

  int max_w = img_w > fb_width ? fb_width : img_w;
  int max_h = img_h > fb_height ? fb_height : img_h;

  uint64_t bpp_bytes = fb_bpp / 8;

  for (int y = 0; y < max_h; ++y)
  {
    uint8_t*             row = fb_addr + y * fb_pitch;
    const unsigned char* src = img + y * img_w * 4;

    for (int x = 0; x < max_w; ++x)
    {
      uint32_t pixel = compose_pixel(src[0], src[1], src[2]);
      uint8_t* dst   = row + x * bpp_bytes;

      if (fb_bpp == 32)
        *(uint32_t*) dst = pixel;
      else if (fb_bpp == 24)
      {
        dst[0] = pixel & 0xFF;
        dst[1] = (pixel >> 8) & 0xFF;
        dst[2] = (pixel >> 16) & 0xFF;
      }

      src += 4;
    }
  }
}
