#include <stdlib.h>
#include <string.h>
#include "coast.h"

#define LAST_TILE 20
/* three quiet blues close to the water colour (set in the palette by the program, see coast.h) */
#define REACH 48                  /* distance in 1/3 pixel: surf only within 16 pixels of the land */

static UBYTE dist[LAST_TILE + 1][256];   /* 0 = land, else distance to the nearest land pixel in 1/3 pixel (255 = none near) */
static UBYTE water[LAST_TILE + 1][256];
static UBYTE shore[LAST_TILE + 1];
static UBYTE plist[LAST_TILE + 1][256], pn[LAST_TILE + 1];   /* the water pixels near the land of each shore tile: the only ones the surf can touch */
static UBYTE crest_pos[COAST_FRAMES];     /* distance of the wave crest from the land in 1/3 pixel */

static int is_water_colour(const UBYTE *palette, int i)
{
    int r = palette[i * 3], g = palette[i * 3 + 1], b = palette[i * 3 + 2];
    return b > 0x80 && b > r + 0x30 && b > g + 0x30;      /* the blues of the sea */
}

void coast_palette(UBYTE *palette)
{
    static const UBYTE rgb[3][3] = { {0x4c, 0x4c, 0xe6}, {0x84, 0x84, 0xee}, {0xb4, 0xb4, 0xf6} };   /* water is 66 66 e6 */
    memcpy(palette + COAST_PEN_DARK * 3, rgb, 9);
}

void coast_init(const UBYTE *tiles, const UBYTE *palette)
{
    UBYTE wc[256];
    for (int i = 0; i < 256; i++) wc[i] = (UBYTE)is_water_colour(palette, i);
    memset(shore, 0, sizeof shore);
    for (int t = 2; t <= LAST_TILE; t++) {
        UBYTE *d = dist[t];
        int land = 0;
        for (int i = 0; i < 256; i++) {
            water[t][i] = wc[tiles[t * 256 + i]];
            if (water[t][i]) d[i] = 255; else { d[i] = 0; land = 1; }
        }
        if (!land) continue;                                /* open water: nothing to animate */
        shore[t] = 1;
        for (int y = 0; y < 16; y++)                        /* chamfer distance 3 / 4: forward pass */
            for (int x = 0; x < 16; x++) {
                int v = d[y * 16 + x], n;
                if (x > 0 && (n = d[y * 16 + x - 1] + 3) < v) v = n;
                if (y > 0 && (n = d[(y - 1) * 16 + x] + 3) < v) v = n;
                if (x > 0 && y > 0 && (n = d[(y - 1) * 16 + x - 1] + 4) < v) v = n;
                if (x < 15 && y > 0 && (n = d[(y - 1) * 16 + x + 1] + 4) < v) v = n;
                d[y * 16 + x] = (UBYTE)(v > 255 ? 255 : v);
            }
        for (int y = 15; y >= 0; y--)                       /* backward pass */
            for (int x = 15; x >= 0; x--) {
                int v = d[y * 16 + x], n;
                if (x < 15 && (n = d[y * 16 + x + 1] + 3) < v) v = n;
                if (y < 15 && (n = d[(y + 1) * 16 + x] + 3) < v) v = n;
                if (x < 15 && y < 15 && (n = d[(y + 1) * 16 + x + 1] + 4) < v) v = n;
                if (x > 0 && y < 15 && (n = d[(y + 1) * 16 + x - 1] + 4) < v) v = n;
                d[y * 16 + x] = (UBYTE)(v > 255 ? 255 : v);
            }
    }
    for (int t = 2; t <= LAST_TILE && t <= LAST_TILE; t++) {
        pn[t] = 0;
        if (!shore[t]) continue;
        for (int i = 0; i < 256; i++) if (water[t][i] && dist[t][i] <= REACH) plist[t][pn[t]++] = (UBYTE)i;
    }
    for (int t = 0; t < 8; t++) {                           /* approach: the crest slows down towards the shore (shoaling) */
        int r = 8 - t;                                       /* 8..1 */
        crest_pos[t] = (UBYTE)(7 + 41 * r * r / 64);         /* 48 .. 7 */
    }
    for (int t = 8; t < COAST_FRAMES; t++) crest_pos[t] = 6;
}

int coast_has(int tile) { return tile >= 2 && tile <= LAST_TILE && shore[tile]; }

static unsigned hash2(int a, int b)
{
    unsigned h = (unsigned)a * 374761393u + (unsigned)b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void paint(UBYTE *dst, int stride, int tile, int wx, int wy, int t, int c)
{
    const UBYTE *d = dist[tile];
    for (int k = 0; k < pn[tile]; k++) {
        {
            int i = plist[tile][k], x = i & 15, y = i >> 4, dd = d[i], px = wx + x, py = wy + y, col = 0;
            int sparse = ((px + 2 * py) % 3) == 0;                   /* only a third of the pixels get the bright foam colour */
            if (t < 8) {                                    /* swell approaching: a crest that gets brighter and wider */
                int len = 3 + t * 2 / 7;                    /* 3..4 (1/3 pixel): thin lines */
                int off = dd - crest_pos[t];
                if (off >= 0 && off < len) {
                    int a = ((px + 2 * py) + t * 3 + c * 17) & 63;      /* groups of waves that drift along the coast */
                    if (a < 44) col = t < 3 ? COAST_PEN_DARK : sparse && t >= 6 ? COAST_PEN_FOAM : COAST_PEN_MID;
                }
            } else if (t < 10) {                            /* break: a band of foam along the land */
                int wide = t == 8 ? 6 : 8;
                int a = ((px + 2 * py) + 24 + c * 17) & 63;
                if (dd >= 3 && dd < wide && a < 44) col = sparse ? COAST_PEN_FOAM : COAST_PEN_MID;
                else if (dd >= 3 && dd < 5) col = COAST_PEN_MID;
            } else {                                        /* backwash: the foam runs back to sea in streaks (rip currents) */
                int rip = (hash2((px >> 2) + c * 3, py >> 2) & 3) == 0 || (hash2(px >> 2, (py >> 2) + c * 5) & 3) == 0;
                int out = 6 + (t - 9) * 6;                  /* how far out the streak has got */
                if (rip && dd >= 6 && dd < out + 2 && dd >= out - 3) col = COAST_PEN_MID;
                else if (t == 10 && dd >= 3 && dd < 5) col = COAST_PEN_MID;
            }
            if (col) dst[y * stride + x] = (UBYTE)col;
        }
    }
}

/* The surf of a tile only depends on the tile, the step of the cycle and one of four variants (chosen from the position and
 * the cycle number), so every picture is made once and later just copied: drawing a shore tile costs one 16x16 copy instead
 * of a loop over its water pixels (on a 68030 the loop took about half of the processor time). */
#define VARIANTS 4
#define CACHE_MAX (200 * 1024L)
static UBYTE *cache[LAST_TILE + 1][COAST_FRAMES][VARIANTS];
static long cache_bytes;

void coast_draw(UBYTE *dst, int stride, int tile, int wx, int wy, int tick)
{
    int t = tick % COAST_FRAMES, c = tick / COAST_FRAMES;
    int v = (int)(hash2((wx >> 4) + c * 5, (wy >> 4) + c * 3) & (VARIANTS - 1));
    UBYTE *e = cache[tile][t][v];
    if (!e && cache_bytes + 256 <= CACHE_MAX && (e = (UBYTE *)malloc(256)) != NULL) {
        for (int y = 0; y < 16; y++) memcpy(e + y * 16, dst + y * stride, 16);        /* the tile as it is now (picture without surf) */
        paint(e, 16, tile, v * 48 + 5, v * 80 + 3, t, v);                             /* fixed pattern per variant */
        cache[tile][t][v] = e; cache_bytes += 256;
    }
    if (e) { for (int y = 0; y < 16; y++) memcpy(dst + y * stride, e + y * 16, 16); return; }
    paint(dst, stride, tile, wx, wy, t, c);                                           /* cache full: draw it the slow way */
}
