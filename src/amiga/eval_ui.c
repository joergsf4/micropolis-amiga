/* City evaluation panel: public opinion, worst problems, statistics and score (mirrors upstream
 * res/wscore.tcl; the numbers arrive as "UISetEvaluation" notifications from w_eval.c). */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdio.h>
#include <string.h>
#include "l10n.h"
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
    {   char tt[80];                                          /* "City Evaluation  2000": translate the words, keep the year */
        if (!strncmp(f[17], "City Evaluation", 15)) sprintf(tt, "%s%s", T("City Evaluation"), f[17] + 15);
        else strcpy(tt, f[17][0] ? T(f[17]) : T("City Evaluation"));
        gfx_print(x, EVAL_Y + 20, tt);
    }
    gfx_colors(PEN_TEXT, PEN_BAR);
    y = EVAL_Y + 48;
    gfx_print(x, y, T("Public opinion: is the mayor doing a good job?"));           y += 16;
    sprintf(b, "   %s %-5s  %s %-5s", T("Yes"), f[15], T("No"), f[16]);                         gfx_print(x, y, b);   y += 24;
    gfx_print(x, y, T("The worst problems:"));                                      y += 16;
    for (int i = 0; i < 4; i++) {
        gfx_print(x + 24, y, T(f[2 + i])); gfx_print_r(x + 24 + 24 * 8, y, f[6 + i]); y += 14;
    }
    y += 10;
    gfx_print(x, y, T("Statistics:"));                                              y += 16;
    gfx_print(x + 24, y, T("Population")); gfx_print_r(x + 24 + 24 * 8, y, f[10]); y += 14;
    gfx_print(x + 24, y, T("Net migration")); gfx_print_r(x + 24 + 24 * 8, y, f[11]); y += 14;
    gfx_print(x + 24, y, T("Assessed value")); gfx_print_r(x + 24 + 24 * 8, y, f[12]); y += 14;
    gfx_print(x + 24, y, T("Category")); gfx_print_r(x + 24 + 24 * 8, y, T(f[13])); y += 14;
    gfx_print(x + 24, y, T("Game level")); gfx_print_r(x + 24 + 24 * 8, y, T(f[14])); y += 20;
    sprintf(b, T("Overall score %s   (annual change %s)"), f[1], f[0]);           gfx_print(x, y, b);
    gfx_colors(PEN_TEXT, PEN_SEL);
    gfx_print_c(EVAL_X + EVAL_W / 2, EVAL_Y + EVAL_H - 17, T("Close"));
}

void eval_close(void) { eval_visible = 0; }

int eval_click(int x, int y)
{
    if (x >= EVAL_X + EVAL_W / 2 - 50 && x < EVAL_X + EVAL_W / 2 + 50 && y >= EVAL_Y + EVAL_H - 34 && y < EVAL_Y + EVAL_H - 10)
        eval_close();
    return 1;
}
