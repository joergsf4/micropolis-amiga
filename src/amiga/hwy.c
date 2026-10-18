#include <string.h>
#include "hwy.h"

#define P_LAND   1             /* game palette entries */
#define P_ASPHALT 16           /* 5d5d5d: a little lighter than the roads' 3f3f3f */
#define P_EDGE   12            /* yellow */
#define P_DASH   10            /* white */
#define P_MEDIAN 7             /* grey concrete */

static int covered(int mask, int x, int y)          /* is this pixel part of the carriageway? */
{
    if (x >= 1 && x <= 14 && y >= 1 && y <= 14) return 1;                   /* centre */
    if ((mask & 1) && y < 1 && x >= 1 && x <= 14) return 1;                  /* north arm */
    if ((mask & 4) && y > 14 && x >= 1 && x <= 14) return 1;
    if ((mask & 2) && x > 14 && y >= 1 && y <= 14) return 1;
    if ((mask & 8) && x < 1 && y >= 1 && y <= 14) return 1;
    return 0;
}

void hwy_make_tiles(UBYTE *dst)
{
    for (int mask = 0; mask < 16; mask++) {
        UBYTE *t = dst + mask * 256;
        int arms = mask ? mask : 5;                      /* a single piece looks like a straight one */
        int junction = ((arms & 5) && (arms & 10));      /* both axes: the middle stays plain asphalt */
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int c = P_LAND;
                if (covered(mask, x, y)) {
                    c = P_ASPHALT;
                    if (!covered(mask, x - 1, y) || !covered(mask, x + 1, y) || !covered(mask, x, y - 1) || !covered(mask, x, y + 1)) c = P_EDGE;
                }
                t[y * 16 + x] = (UBYTE)c;
            }
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int mid = x >= 5 && x <= 10 && y >= 5 && y <= 10;
                if (junction && mid) continue;
                if (t[y * 16 + x] != P_ASPHALT) continue;
                if (((arms & 1) && y <= 7) || ((arms & 4) && y >= 8)) {           /* vertical arms: median at x 7..8, dashes at 4 and 11 */
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
