#ifndef COAST_H
#define COAST_H
/* Animated surf along the shore lines: drawn over the shore tiles (tile numbers 2..20) after the tile picture is copied.
 *  - waves run towards the shore and slow down on the way (shoaling: the crest piles up, grows brighter and wider)
 *  - the wave fronts follow the shape of the coast, because the phase depends on the distance to the land (refraction)
 *  - the waves arrive in groups that drift along the coast (longshore drift)
 *  - after the break the foam runs back to sea in a few streaks (rip currents)
 * A cycle has COAST_FRAMES pictures. */
#include <exec/types.h>
#define COAST_FRAMES 12
#define COAST_PEN_DARK 44        /* palette entries for the surf: a little darker, a little lighter and a light blue than the water */
#define COAST_PEN_MID  45
#define COAST_PEN_FOAM 46
void coast_palette(UBYTE *palette);   /* sets those three entries in a 256 x RGB palette */
void coast_init(const UBYTE *tiles, const UBYTE *palette);   /* analyses the tile pictures (water / land pixels, distance to the land) */
int  coast_has(int tile);                                    /* 1 when the tile is shore with animated surf */
void coast_draw(UBYTE *dst, int stride, int tile, int wx, int wy, int tick);   /* dst = the 16x16 tile already copied; (wx, wy) = world pixel of its corner */
#endif
