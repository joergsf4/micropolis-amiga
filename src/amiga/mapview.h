#ifndef MAPVIEW_H
#define MAPVIEW_H
#include <exec/types.h>
#define MAP_PX 3                                   /* pixels per tile */
#define MV_W  (120 * MAP_PX)
#define MV_H  (100 * MAP_PX)
extern int map_visible;
extern int map_mode;                               /* index into map_modes[] */
extern const char *const map_names[];
#define MAP_MODES 14
int  map_load(void);                               /* res/tilessm.bin */
void map_refresh(void);                            /* redraw the map bitmap from the simulation */
void map_blit(UBYTE *fb, int stride, int x0, int y0, int view_x, int view_y, int cols, int rows);
int  map_hit(int x, int y, int x0, int y0, int *tx, int *ty);   /* pixel -> tile if inside the map */
const UBYTE *map_buffer(void);
#endif
