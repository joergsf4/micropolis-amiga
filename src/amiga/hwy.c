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

/* Cars: little 2x3 pixel cars, one every 44 pixels per lane (two thirds of the slots), 2 pixels per step; the left carriageway runs one way, the right
 * one the other, two lanes each. The pattern hangs on the world position, so the cars run on across tile borders. Only plain
 * asphalt is painted over. (A "mouse cinema" like the traffic on the ordinary roads, not big blocks.) */
void hwy_cars(UBYTE *dst, int stride, int tile, int wx, int wy, int tick)
{
    static const UBYTE col[2] = { 10, 14 };                         /* white and light grey: no colours, like the traffic on the roads */
    int mask = tile - 960, arms = mask ? mask : 5;
    int junction = ((arms & 5) && (arms & 10));
    int off = (tick & 0xFFFFF) * 2 % 44;                              /* how far the cars have moved, never negative */
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int px = wx + x, py = wy + y, pos, c, lane = -1, a;
            UBYTE *d = dst + y * stride + x;
            if (*d != P_ASPHALT) continue;
            if (junction && x >= 4 && x <= 11 && y >= 4 && y <= 11) continue;
            if (((arms & 1) && y <= 7) || ((arms & 4) && y >= 8)) {              /* vertical arms: 4 lanes at x 2-3, 5-6, 9-10, 12-13 */
                if (x == 2 || x == 3) lane = 0; else if (x == 5 || x == 6) lane = 1; else if (x == 9 || x == 10) lane = 2; else if (x == 12 || x == 13) lane = 3;
                if (lane >= 0) {
                    a = (lane < 2) ? py + off : py - off + 44;                          /* left carriageway down, right one up */
                    a += lane * 19;
                    pos = a % 44; c = a / 44;
                    if (pos < 3 && ((c * 7 + lane * 5 + (px >> 5)) % 3) != 0) *d = col[(c + lane) & 1];
                }
            }
            if (lane < 0 && (((arms & 2) && x >= 8) || ((arms & 8) && x <= 7))) {    /* horizontal arms: lanes at y 2-3, 5-6, 9-10, 12-13 */
                if (y == 2 || y == 3) lane = 0; else if (y == 5 || y == 6) lane = 1; else if (y == 9 || y == 10) lane = 2; else if (y == 12 || y == 13) lane = 3;
                if (lane >= 0) {
                    a = (lane < 2) ? px - off + 44 : px + off;                          /* upper carriageway west, lower one east */
                    a += lane * 19;
                    pos = a % 44; c = a / 44;
                    if (pos < 3 && ((c * 7 + lane * 5 + (py >> 5)) % 3) != 0) *d = col[(c + lane) & 1];
                }
            }
        }
}

void hwy_small_tiles(UBYTE *dst)
{
    memset(dst, P_ASPHALT, HWY_TILES * 12);
}
