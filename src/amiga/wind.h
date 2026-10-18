#ifndef WIND_H
#define WIND_H
/* Gusts of wind over the woods: a slow, wide band of slightly lighter green travels across the map. Only the bright
 * green pixels of the trees inside the band change, dithered, so the forest keeps its texture and the picture stays calm.
 * The band depends on the world position (not on the tile), so neighbouring forest tiles move together. One step is
 * one tick of the animation clock (the same one as the surf). */
#include <exec/types.h>
#define WIND_PEN 47                                 /* palette entry of the lighter green */
void wind_init(const UBYTE *tiles);                 /* finds the tree tiles */
void wind_palette(UBYTE *palette);
int  wind_has(int tile);
int  wind_active(int wx, int wy, int tick);         /* 1 when the tile at this world pixel is (about to be) touched by a gust in this step */
void wind_draw(UBYTE *dst, int stride, int tile, int wx, int wy, int tick);
#endif
