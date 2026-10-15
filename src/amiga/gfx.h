#ifndef GFX_H
#define GFX_H
#include <exec/types.h>
/* Text drawn straight into the 8 bit frame buffer with a built-in 8x8 font. This keeps the whole picture
 * in one buffer, so it works on every RTG screen depth (8, 15/16, 24/32 bit) without pens or RastPort text. */
void gfx_target(UBYTE *fb, int stride);
void gfx_colors(UBYTE fg, UBYTE bg);
void gfx_print_c(int cx, int baseline, const char *s);     /* centred on cx */
void gfx_print_r(int right, int baseline, const char *s);  /* ending at x = right */
void gfx_print(int x, int baseline, const char *s);     /* baseline like Text(): glyphs occupy baseline-7 .. baseline */
#endif
