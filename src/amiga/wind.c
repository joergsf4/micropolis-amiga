#include <string.h>
#include "wind.h"

#define LAST_TILE 63
#define PEN_BRIGHT 8              /* the tile palette's bright green: the only colour that changes */
#define SPACING 960               /* distance between two gusts (world pixels) */
#define BAND 320                  /* width of a gust */
#define SPEED 4                   /* world pixels per step */
#define PEAK 7                    /* strongest dithering: 7 of 16 bright pixels (14 was far too restless, 5 was not seen) */

static UBYTE tree[LAST_TILE + 1];
static const UBYTE bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

void wind_init(const UBYTE *tiles)
{
    for (int t = 0; t <= LAST_TILE; t++) {                  /* trees: mostly green texture (bright 8 and dark 9 pixels) */
        int n = 0;
        for (int i = 0; i < 256; i++) { UBYTE c = tiles[t * 256 + i]; if (c == PEN_BRIGHT || c == 9) n++; }
        tree[t] = n >= 100;
    }
}

void wind_palette(UBYTE *palette)
{
    static const UBYTE rgb[3] = { 0x90, 0xf8, 0x78 };       /* the bright green is 00 e6 00 */
    memcpy(palette + WIND_PEN * 3, rgb, 3);
}

int wind_has(int tile) { return tile >= 0 && tile <= LAST_TILE && tree[tile]; }

static int wave(int wx, int wy, int tick)                    /* position inside the repeating gust pattern, 0..SPACING-1 */
{
    int tri = ((wy >> 2) & 63) - 32;
    int s = wx + (wy >> 1) + 2 * (tri < 0 ? -tri : tri);     /* slightly bent front */
    int u = (s - tick * SPEED) % SPACING;
    return u < 0 ? u + SPACING : u;
}

int wind_active(int wx, int wy, int tick)
{
    int u = wave(wx + 8, wy + 8, tick);                      /* tile centre; margin for the tilted front */
    return u < BAND + 40 || u >= SPACING - 40;
}

void wind_draw(UBYTE *dst, int stride, int tile, int wx, int wy, int tick)
{
    for (int by = 0; by < 4; by++)                          /* the strength is worked out once for each block of 4x4 pixels */
        for (int bx = 0; bx < 4; bx++) {
            int u = wave(wx + bx * 4 + 2, wy + by * 4 + 2, tick), d, k;
            if (u >= BAND) continue;
            d = u < BAND / 2 ? u : BAND - u;                 /* 0 at the edges, BAND/2 in the middle */
            k = d * PEAK * 2 / BAND;                         /* 0..PEAK, soft edges */
            if (k <= 0) continue;
            for (int y = by * 4; y < by * 4 + 4; y++)
                for (int x = bx * 4; x < bx * 4 + 4; x++)
                    if (dst[y * stride + x] == PEN_BRIGHT && bayer[((wy + y) & 3) * 4 + ((wx + x) & 3)] < k) dst[y * stride + x] = WIND_PEN;
        }
    (void)tile;
}
