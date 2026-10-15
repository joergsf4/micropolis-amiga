#ifndef GRAPH_UI_H
#define GRAPH_UI_H
#include <exec/types.h>
extern int graph_visible;
#define GRAPH_X 118
#define GRAPH_Y 78
#define GRAPH_W 404
#define GRAPH_H 324
void graph_draw(UBYTE *fb, int stride);      /* panel graphics and text (gfx_target is set by the caller) */
int  graph_click(int x, int y);              /* returns 1 (modal); changes range / series */
void graph_close(void);
#endif
