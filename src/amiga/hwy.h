#ifndef HWY_H
#define HWY_H
/* Pictures of the highway tiles (core tiles HWYBASE..HWYLAST): drawn in code, 16 combinations of connected sides
 * (N 1, E 2, S 4, W 8). Asphalt, yellow edge lines, white dashes either side of a concrete median. */
#include <exec/types.h>
#define HWY_TILES 16
void hwy_make_tiles(UBYTE *dst);     /* 16 tiles of 16x16 palette indices */
void hwy_cars(UBYTE *dst, int stride, int tile, int wx, int wy, int tick);   /* little cars on a highway tile already copied into dst; (wx, wy) = world pixel of its corner */
void hwy_small_tiles(UBYTE *dst);    /* 16 minimap tiles of 12 bytes (2x? colours like the game's tilessm.bin: same pen for all) */
#endif
