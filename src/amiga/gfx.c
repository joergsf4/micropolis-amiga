#include <string.h>
#include "gfx.h"
#include "font8x8_basic.h"          /* public domain (Daniel Hepper, from IBM VGA font) */
#include "font8x8_latin1.h"

static UBYTE *tfb;
static int tstride;
static UBYTE fg_pen = 1, bg_pen = 0;

void gfx_target(UBYTE *fb, int stride) { tfb = fb; tstride = stride; }
void gfx_colors(UBYTE fg, UBYTE bg) { fg_pen = fg; bg_pen = bg; }

#define GLYPH(c) latin1_glyph((unsigned char)(c))
static const char *latin1_glyph(unsigned char c)               /* ASCII from the base font, a few Latin-1 letters from font8x8_latin1.h */
{
    if (c < 128) return font8x8_basic[c];
    for (int i = 0; i < (int)(sizeof latin1_codes); i++) if (latin1_codes[i] == c) return latin1_glyphs[i];
    return font8x8_basic['?'];
}

void gfx_print_c(int cx, int baseline, const char *s) { gfx_print(cx - 4 * (int)strlen(s), baseline, s); }
void gfx_print_r(int right, int baseline, const char *s) { gfx_print(right - 8 * (int)strlen(s), baseline, s); }

void gfx_print(int x, int baseline, const char *s)
{
    int y0 = baseline - 7;
    for (; *s; s++, x += 8) {
        const char *g = GLYPH(*s);
        for (int row = 0; row < 8; row++) {
            UBYTE *d = tfb + (y0 + row) * tstride + x;
            unsigned bits = (unsigned char)g[row];
            for (int col = 0; col < 8; col++, bits >>= 1) d[col] = (bits & 1) ? fg_pen : bg_pen;
        }
    }
}
