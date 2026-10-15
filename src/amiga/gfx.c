#include <string.h>
#include "gfx.h"
#include "font8x8_basic.h"          /* public domain (Daniel Hepper, from IBM VGA font) */

static UBYTE *tfb;
static int tstride;
static UBYTE fg_pen = 1, bg_pen = 0;

void gfx_target(UBYTE *fb, int stride) { tfb = fb; tstride = stride; }
void gfx_colors(UBYTE fg, UBYTE bg) { fg_pen = fg; bg_pen = bg; }

void gfx_print(int x, int baseline, const char *s)
{
    int y0 = baseline - 7;
    for (; *s; s++, x += 8) {
        const char *g = font8x8_basic[(unsigned char)*s & 0x7f];
        for (int row = 0; row < 8; row++) {
            UBYTE *d = tfb + (y0 + row) * tstride + x;
            unsigned bits = (unsigned char)g[row];
            for (int col = 0; col < 8; col++, bits >>= 1) d[col] = (bits & 1) ? fg_pen : bg_pen;
        }
    }
}
