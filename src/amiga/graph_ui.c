/* History graphs: residential, commercial, industrial, money, crime and pollution over the last
 * 10 years (monthly) or 120 years (yearly), as in upstream's graph window. The data is the core's
 * History10/History120 (120 values 0..255 each, oldest first), kept up to date by graphDoer(). */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <string.h>
#include <stdio.h>
#include "l10n.h"
#include "gfx.h"
#include "graph_ui.h"

int graph_visible;

static int range = 10;                     /* 10 or 120 years */
static int mask = 0x3f;                    /* shown series */
static const char *const names[6] = { N_("Residential"), N_("Commercial"), N_("Industrial"), N_("Money"), N_("Crime"), N_("Pollution") };
static const UBYTE pens[6] = { PEN_V_LGREEN, PEN_V_LBLUE, PEN_V_YELLOW, PEN_TEXT, PEN_V_RED, PEN_V_ORANGE };

#define PX (GRAPH_X + 20)
#define PY (GRAPH_Y + 40)
#define PW 360
#define PH 160

static void rect(UBYTE *fb, int stride, int x, int y, int w, int h, UBYTE c)
{
    for (int j = 0; j < h; j++) memset(fb + (y + j) * stride + x, c, w);
}

static void line(UBYTE *fb, int stride, int x0, int y0, int x1, int y1, UBYTE c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, e2;
    for (;;) {
        fb[y0 * stride + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;                                  /* decide on the old error for both steps */
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void graph_draw(UBYTE *fb, int stride)
{
    char b[40];
    rect(fb, stride, GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, PEN_TEXT);
    rect(fb, stride, GRAPH_X + 2, GRAPH_Y + 2, GRAPH_W - 4, GRAPH_H - 4, PEN_BAR);
    rect(fb, stride, GRAPH_X + 2, GRAPH_Y + 2, GRAPH_W - 4, 26, PEN_BTN);
    gfx_colors(PEN_TEXT, PEN_BTN);
    sprintf(b, T("History graphs - last %d years"), range);
    gfx_print(GRAPH_X + 12, GRAPH_Y + 20, b);
    /* plot area with grid */
    rect(fb, stride, PX - 1, PY - 1, PW + 2, PH + 2, PEN_TEXT);
    rect(fb, stride, PX, PY, PW, PH, PEN_BTN);
    for (int g = 1; g < 4; g++) memset(fb + (PY + g * PH / 4) * stride + PX, PEN_BAR, PW);
    for (int s = 0; s < 6; s++) {
        const unsigned char *h;
        if (!(mask & (1 << s)) || !(h = (range == 10 ? History10[s] : History120[s]))) continue;
        for (int i = 0; i < 119; i++) {
            int x0 = PX + i * (PW - 1) / 119, x1 = PX + (i + 1) * (PW - 1) / 119;
            int y0 = PY + PH - 1 - h[i] * (PH - 1) / 255, y1 = PY + PH - 1 - h[i + 1] * (PH - 1) / 255;
            line(fb, stride, x0, y0, x1, y1, pens[s]);
        }
    }
    /* range buttons and series toggles */
    rect(fb, stride, GRAPH_X + 20, GRAPH_Y + GRAPH_H - 82, 70, 20, range == 10 ? PEN_SEL : PEN_BTN);
    rect(fb, stride, GRAPH_X + 98, GRAPH_Y + GRAPH_H - 82, 70, 20, range == 120 ? PEN_SEL : PEN_BTN);
    gfx_colors(PEN_TEXT, range == 10 ? PEN_SEL : PEN_BTN);  gfx_print_c(GRAPH_X + 55, GRAPH_Y + GRAPH_H - 67, T("10 yrs"));
    gfx_colors(PEN_TEXT, range == 120 ? PEN_SEL : PEN_BTN); gfx_print_c(GRAPH_X + 133, GRAPH_Y + GRAPH_H - 67, T("120 yrs"));
    for (int s = 0; s < 6; s++) {
        int x = GRAPH_X + 20 + (s % 3) * 130, y = GRAPH_Y + GRAPH_H - 50 + (s / 3) * 20;
        rect(fb, stride, x, y, 12, 12, (mask & (1 << s)) ? pens[s] : PEN_BTN);
        gfx_colors(PEN_TEXT, PEN_BAR);
        gfx_print(x + 18, y + 10, T(names[s]));
    }
    rect(fb, stride, GRAPH_X + GRAPH_W - 94, GRAPH_Y + GRAPH_H - 82, 76, 20, PEN_SEL);
    gfx_colors(PEN_TEXT, PEN_SEL);
    gfx_print_c(GRAPH_X + GRAPH_W - 56, GRAPH_Y + GRAPH_H - 67, T("Close"));
}

void graph_close(void) { graph_visible = 0; }

int graph_click(int x, int y)
{
    int by = GRAPH_Y + GRAPH_H - 82;
    if (y >= by && y < by + 20) {
        if (x >= GRAPH_X + 20 && x < GRAPH_X + 90) range = 10;
        else if (x >= GRAPH_X + 98 && x < GRAPH_X + 168) range = 120;
        else if (x >= GRAPH_X + GRAPH_W - 94 && x < GRAPH_X + GRAPH_W - 18) graph_close();
    }
    for (int s = 0; s < 6; s++) {
        int sx = GRAPH_X + 20 + (s % 3) * 130, sy = GRAPH_Y + GRAPH_H - 50 + (s / 3) * 20;
        if (x >= sx && x < sx + 130 && y >= sy - 2 && y < sy + 16) mask ^= 1 << s;
    }
    return 1;
}
