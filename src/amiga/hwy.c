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

void hwy_small_tiles(UBYTE *dst)
{
    memset(dst, P_ASPHALT, HWY_TILES * 12);
}
