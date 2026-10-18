#include <string.h>
#include "hwy.h"

#define P_LAND   1             /* game palette entries */
#define P_ASPHALT 2            /* 3f3f3f like the roads */
#define P_EDGE   10            /* white edge line */
#define P_DASH   10            /* white */
#define P_BARRIER 7            /* grey crash barrier on the closed sides */
#define P_MEDIAN 7             /* grey concrete */

/* A highway fills the whole tile (a road is only 12 pixels wide): two lanes each way, a grey median, a white edge line
 * and a crash barrier on every side that does not continue. */
void hwy_make_tiles(UBYTE *dst)
{
    for (int mask = 0; mask < 16; mask++) {
        UBYTE *t = dst + mask * 256;
        int arms = mask ? mask : 5;                      /* a single piece looks like a straight one */
        int junction = ((arms & 5) && (arms & 10));      /* both axes: the middle stays plain asphalt */
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int d = 99;                              /* distance to the nearest closed side */
                if (!(mask & 1) && y < d) d = y;
                if (!(mask & 4) && 15 - y < d) d = 15 - y;
                if (!(mask & 2) && 15 - x < d) d = 15 - x;
                if (!(mask & 8) && x < d) d = x;
                t[y * 16 + x] = (UBYTE)(d == 0 ? P_BARRIER : d == 1 ? P_EDGE : P_ASPHALT);
            }
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int mid = x >= 4 && x <= 11 && y >= 4 && y <= 11;
                if (junction && mid) continue;
                if (t[y * 16 + x] != P_ASPHALT) continue;
                if (((arms & 1) && y <= 7) || ((arms & 4) && y >= 8)) {           /* vertical arms: median at x 7..8, lane dashes at 4 and 11 */
                    if (x == 7 || x == 8) t[y * 16 + x] = P_MEDIAN;
                    else if ((x == 4 || x == 11) && ((y >> 2) & 1) == 0) t[y * 16 + x] = P_DASH;
                }
                if (((arms & 2) && x >= 8) || ((arms & 8) && x <= 7)) {           /* horizontal arms */
                    if (y == 7 || y == 8) t[y * 16 + x] = P_MEDIAN;
                    else if ((y == 4 || y == 11) && ((x >> 2) & 1) == 0) t[y * 16 + x] = P_DASH;
                }
            }
    }
}

/* Cars: a car every 40 pixels in each lane, 3 pixels per step, left lane one way, right lane the other. The pattern hangs on
 * the world position, so the cars run on across tile borders. Only plain asphalt is painted over. */
void hwy_cars(UBYTE *dst, int stride, int tile, int wx, int wy, int tick)
{
    static const UBYTE col[4] = { 11, 12, 10, 17 };                 /* red, yellow, white, blue */
    int mask = tile - 960, arms = mask ? mask : 5;
    int junction = ((arms & 5) && (arms & 10));
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int px = wx + x, py = wy + y, lane = -1, s, c;
            UBYTE *d = dst + y * stride + x;
            if (*d != P_ASPHALT) continue;
            if (junction && x >= 4 && x <= 11 && y >= 4 && y <= 11) continue;
            if (((arms & 1) && y <= 7) || ((arms & 4) && y >= 8)) {              /* vertical arms */
                if (x >= 3 && x <= 5) { lane = 0; s = (py + tick * 3) % 40; c = (py + tick * 3) / 40; }          /* left lane: down */
                else if (x >= 10 && x <= 12) { lane = 1; s = ((py - tick * 3 + 4000) + 21) % 40; c = ((py - tick * 3 + 4000) + 21) / 40; }
                else s = 99, c = 0;
                if (lane >= 0 && s < 5) *d = col[(c + lane * 2 + (px >> 4)) & 3];
            }
            if (lane < 0 && (((arms & 2) && x >= 8) || ((arms & 8) && x <= 7))) {    /* horizontal arms */
                if (y >= 3 && y <= 5) { lane = 0; s = ((px - tick * 3 + 4000) + 13) % 40; c = ((px - tick * 3 + 4000) + 13) / 40; }       /* upper lane: west */
                else if (y >= 10 && y <= 12) { lane = 1; s = (px + tick * 3) % 40; c = (px + tick * 3) / 40; }
                else s = 99, c = 0;
                if (lane >= 0 && s < 5) *d = col[(c + lane * 2 + (py >> 4)) & 3];
            }
        }
}

void hwy_small_tiles(UBYTE *dst)
{
    memset(dst, P_ASPHALT, HWY_TILES * 12);
}
