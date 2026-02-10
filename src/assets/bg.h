/* This file exposes the assets generated from files in assets/.
   Depending on the input file the symbols can be either for BMP or JPG
   (e.g. `assets_bg_bmp`/`assets_bg_bmp_len` or `assets_bg_jpg`/`assets_bg_jpg_len`).
   Declare both variants plus convenient aliases so code can use either. */

#ifndef ASSETS_BG_H
#define ASSETS_BG_H

/* JPEG-backed symbols (present when `assets/bg.jpg` is converted) */
extern unsigned char assets_bg_jpg[];
extern unsigned int assets_bg_jpg_len;
extern unsigned char bg_jpg[];
extern unsigned int bg_jpg_len;

/* BMP-backed symbols (present when `assets/bg.bmp` is converted) */
extern unsigned char assets_bg_bmp[];
extern unsigned int assets_bg_bmp_len;
/* Convenience pointer for simple use */
extern unsigned char *bg_bmp;

#endif
