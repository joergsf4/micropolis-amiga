/* Native preview of the surf animation: renders a part of a city file for every phase of the cycle into one PPM.
 * cc -O1 -I src/amiga -o out/coast/preview tools/coast_preview.c src/amiga/coast.c
 * out/coast/preview out/data/tiles.bin out/data/palette.bin upstream/cities/haight.cty X0 Y0 W H out.ppm */
#include <stdio.h>
#include <stdlib.h>
#include "coast.h"
static unsigned char *rd(const char *p, long *n) { FILE *f = fopen(p, "rb"); unsigned char *b; fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f); b = malloc(*n); fread(b, 1, *n, f); fclose(f); return b; }
int main(int argc, char **argv)
{
    long n; unsigned char *tiles = rd(argv[1], &n), *pal = rd(argv[2], &n), *cty = rd(argv[3], &n);
    int x0 = atoi(argv[4]), y0 = atoi(argv[5]), w = atoi(argv[6]), h = atoi(argv[7]);
    int frames = argc > 9 ? atoi(argv[9]) : COAST_FRAMES, cols = 4, rows = (frames + cols - 1) / cols;
    int pw = w * 16, ph = h * 16, W = pw * cols, H = ph * rows;
    unsigned char *img = calloc(W * H, 3), *tile = malloc(pw * ph);
    const unsigned char *map = cty + n - 24000;
    coast_init(tiles, pal);
    for (int f = 0; f < frames; f++) {
        for (int ty = 0; ty < h; ty++) for (int tx = 0; tx < w; tx++) {
            int mx = x0 + tx, my = y0 + ty, v = (map[(mx * 100 + my) * 2] << 8 | map[(mx * 100 + my) * 2 + 1]) & 0x3ff;
            unsigned char *dst = tile + ty * 16 * pw + tx * 16;
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) dst[y * pw + x] = tiles[v * 256 + y * 16 + x];
            if (coast_has(v)) coast_draw(dst, pw, v, mx * 16, my * 16, f);
        }
        for (int y = 0; y < ph; y++) for (int x = 0; x < pw; x++) {
            unsigned char *o = img + (((f / cols) * ph + y) * W + (f % cols) * pw + x) * 3, c = tile[y * pw + x];
            o[0] = pal[c * 3]; o[1] = pal[c * 3 + 1]; o[2] = pal[c * 3 + 2];
        }
    }
    FILE *o = fopen(argv[8], "wb"); fprintf(o, "P6\n%d %d\n255\n", W, H); fwrite(img, 1, W * H * 3, o); fclose(o);
    return 0;
}
