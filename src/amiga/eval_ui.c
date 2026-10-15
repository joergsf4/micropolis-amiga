/* City evaluation panel: public opinion, worst problems, statistics and score (mirrors upstream
 * res/wscore.tcl; the numbers arrive as "UISetEvaluation" notifications from w_eval.c). */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "eval_ui.h"

int eval_visible;

static char f[18][40];       /* changed score p0 p1 p2 p3 v0 v1 v2 v3 pop delta assessed class level yes no title */

void eval_set(const char *cmd)
{
    const char *s = cmd;
    for (int i = 0; i < 18; i++) {
        const char *b = strchr(s, '{'), *e = b ? strchr(b, '}') : NULL;
        size_t n;
        if (!b || !e) { f[i][0] = 0; continue; }
        n = (size_t)(e - b - 1);
        if (n > 39) n = 39;
        memcpy(f[i], b + 1, n); f[i][n] = 0;
        s = e + 1;
    }
}

static void rect(UBYTE *fb, int stride, int x, int y, int w, int h, UBYTE c)
{
    for (int j = 0; j < h; j++) memset(fb + (y + j) * stride + x, c, w);
}

void eval_draw(UBYTE *fb, int stride)
{
    char b[80];
    int x = EVAL_X + 16, y;
    rect(fb, stride, EVAL_X, EVAL_Y, EVAL_W, EVAL_H, PEN_TEXT);
    rect(fb, stride, EVAL_X + 2, EVAL_Y + 2, EVAL_W - 4, EVAL_H - 4, PEN_BAR);
    rect(fb, stride, EVAL_X + 2, EVAL_Y + 2, EVAL_W - 4, 26, PEN_BTN);
    rect(fb, stride, EVAL_X + EVAL_W / 2 - 50, EVAL_Y + EVAL_H - 34, 100, 24, PEN_SEL);
    gfx_colors(PEN_TEXT, PEN_BTN);
    gfx_print(x, EVAL_Y + 20, f[17][0] ? f[17] : "City Evaluation");
    gfx_colors(PEN_TEXT, PEN_BAR);
    y = EVAL_Y + 48;
    gfx_print(x, y, "Public opinion: is the mayor doing a good job?");           y += 16;
    sprintf(b, "   Yes %-5s  No %-5s", f[15], f[16]);                         gfx_print(x, y, b);   y += 24;
    gfx_print(x, y, "The worst problems:");                                      y += 16;
    for (int i = 0; i < 4; i++) {
        sprintf(b, "   %-16s %s", f[2 + i], f[6 + i]);
        gfx_print(x, y, b); y += 14;
    }
    y += 10;
    gfx_print(x, y, "Statistics:");                                              y += 16;
    sprintf(b, "   Population     %s", f[10]);                                  gfx_print(x, y, b); y += 14;
    sprintf(b, "   Net migration  %s", f[11]);                                  gfx_print(x, y, b); y += 14;
    sprintf(b, "   Assessed value %s", f[12]);                                  gfx_print(x, y, b); y += 14;
    sprintf(b, "   Category       %s", f[13]);                                  gfx_print(x, y, b); y += 14;
    sprintf(b, "   Game level     %s", f[14]);                                  gfx_print(x, y, b); y += 20;
    sprintf(b, "Overall score %s   (annual change %s)", f[1], f[0]);           gfx_print(x, y, b);
    gfx_colors(PEN_TEXT, PEN_SEL);
    gfx_print(EVAL_X + EVAL_W / 2 - 34, EVAL_Y + EVAL_H - 17, "Close");
}

void eval_close(void) { eval_visible = 0; }

int eval_click(int x, int y)
{
    if (x >= EVAL_X + EVAL_W / 2 - 50 && x < EVAL_X + EVAL_W / 2 + 50 && y >= EVAL_Y + EVAL_H - 34 && y < EVAL_Y + EVAL_H - 10)
        eval_close();
    return 1;
}
