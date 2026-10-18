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

/* Cars: the same little cars as on the ordinary roads (tiles 80 and 81 of the original: 5x3 pixels across, 3x5 lengthwise,
 * white / grey / light grey with a black shadow edge), one lane per carriageway, one car every 24 pixels, a quarter of the
 * places empty. The left carriageway runs one way, the right one the other. The pattern hangs on the world position, so the
 * cars run on across tile borders. */
void hwy_cars(UBYTE *dst, int stride, int tile, int wx, int wy, int tick)
{
    static const UBYTE H[3][5] = { { 10, 7, 7, 14, 3 }, { 10, 7, 7, 14, 3 }, { 3, 3, 3, 3, 3 } };       /* across: 5 wide, 3 high */
    static const UBYTE V[5][3] = { { 10, 10, 3 }, { 7, 7, 3 }, { 7, 7, 3 }, { 14, 14, 3 }, { 3, 3, 3 } };  /* lengthwise: 3 wide, 5 high */
    int mask = tile - 960, arms = mask ? mask : 5;
    int junction = ((arms & 5) && (arms & 10));
    int off = (tick & 0xFFFFF) * 2 % 24;                              /* how far the cars have moved, never negative */
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int px = wx + x, py = wy + y, a, u, c, col, row;
            UBYTE *d = dst + y * stride + x;
            if (junction && x >= 4 && x <= 11 && y >= 4 && y <= 11) continue;
            if (((arms & 1) && y <= 7) || ((arms & 4) && y >= 8)) {              /* vertical arms: lanes at x 3-5 (down) and 10-12 (up) */
                int lane = (x >= 3 && x <= 5) ? 0 : (x >= 10 && x <= 12) ? 1 : -1;
                if (lane >= 0) {
                    a = lane == 0 ? py + off : py - off + 24 + 7;
                    u = a % 24; c = a / 24; col = x - (lane == 0 ? 3 : 10);
                    if (u < 5 && ((c * 7 + lane * 5) % 4) != 0) *d = V[u][col];
                }
            }
            if (((arms & 2) && x >= 8) || ((arms & 8) && x <= 7)) {              /* horizontal arms: lanes at y 3-5 (west) and 10-12 (east) */
                int lane = (y >= 3 && y <= 5) ? 0 : (y >= 10 && y <= 12) ? 1 : -1;
                if (lane >= 0) {
                    a = lane == 0 ? px - off + 24 : px + off + 11;
                    u = a % 24; c = a / 24; row = y - (lane == 0 ? 3 : 10);
                    if (u < 5 && ((c * 7 + lane * 5) % 4) != 0) *d = H[row][u];
                }
            }
        }
}

void hwy_small_tiles(UBYTE *dst)
{
    memset(dst, P_ASPHALT, HWY_TILES * 12);
}
