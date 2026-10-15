#ifndef EVAL_UI_H
#define EVAL_UI_H
#include <exec/types.h>
extern int eval_visible;
#define EVAL_X 118
#define EVAL_Y 78
#define EVAL_W 404
#define EVAL_H 324
void eval_set(const char *cmd);              /* "UISetEvaluation {..} x18": the core sends it whenever the numbers change */
void eval_draw(UBYTE *fb, int stride);       /* panel graphics and text (gfx_target is set by the caller) */
int  eval_click(int x, int y);               /* returns 1 (modal) */
void eval_close(void);
#endif
