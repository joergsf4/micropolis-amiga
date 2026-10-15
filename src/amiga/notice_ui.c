/* Event notices ("pictures" in the core: pollution/crime/traffic alerts, disasters, city growth, win/lose).
 * The core sends "UIShowPicture <id>"; title and text are the original ones, extracted from upstream
 * res/micropolis.tcl into res/notices.txt by tools/convert_assets.py. */
/* sim.h declares a GetSprite() that clashes with graphics.library's */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "l10n.h"
#include "gfx.h"
#include <exec/memory.h>
#include "notice_ui.h"

#define MAX_NOTICES 64
#define MAX_LINES 14
#define LINE_CHARS 50                         /* wrapped line width (8 px font, 440 px panel) */

struct notice { int id; char *title; char *body; };
static struct notice notices[MAX_NOTICES];
static int nnotices;

int notice_visible;
static const struct notice *cur;
static char lines[MAX_LINES][LINE_CHARS + 2];
static int nlines;

int notice_load(void)
{
    BPTR f = Open((CONST_STRPTR)"PROGDIR:res/notices.txt", MODE_OLDFILE);
    char *buf, *p;
    LONG len;
    if (!f) return 0;
    Seek(f, 0, OFFSET_END);
    len = Seek(f, 0, OFFSET_BEGINNING);
    buf = AllocVec(len + 1, MEMF_ANY);
    if (!buf || Read(f, buf, len) != len) { Close(f); return 0; }
    Close(f);
    buf[len] = 0;
    for (p = buf; *p && nnotices < MAX_NOTICES; ) {
        char *eol = strchr(p, '\n'), *t, *b;
        if (!eol) eol = p + strlen(p);
        *eol = 0;
        t = strchr(p, '|');
        b = t ? strchr(t + 1, '|') : NULL;
        if (t && b) {
            *t = 0; *b = 0;
            notices[nnotices].id = atoi(p);
            notices[nnotices].title = t + 1;
            notices[nnotices].body = b + 1;
            nnotices++;
        }
        p = eol + 1;
    }
    return 1;
}

/* word-wrap the body (with "\n" escape sequences for line breaks) into lines[] */
static void wrap(const char *s)
{
    char word[64];
    int col = 0, wl = 0;
    nlines = 0;
    lines[0][0] = 0;
#define NEWLINE() do { if (nlines < MAX_LINES - 1) nlines++; lines[nlines][0] = 0; col = 0; } while (0)
    for (;; s++) {
        int end = (*s == 0), brk = (s[0] == '\\' && s[1] == 'n') || *s == '\n';
        if (*s == ' ' || end || brk) {
            if (wl) {
                word[wl] = 0;
                if (col && col + 1 + wl > LINE_CHARS) NEWLINE();
                if (col) { strcat(lines[nlines], " "); col++; }
                strcat(lines[nlines], word); col += wl; wl = 0;
            }
            if (end) break;
            if (brk) { NEWLINE(); if (*s == '\\') s++; }
        } else if (wl < 60) {
            word[wl++] = *s;
        }
    }
    nlines++;
}

void notice_show(int id)
{
    for (int i = 0; i < nnotices; i++) {
        if (notices[i].id == id) {
            cur = &notices[i];
            wrap(T(cur->body));
            notice_visible = 1;
            return;
        }
    }
}

void notice_show_text(const char *title, const char *text)       /* free text instead of a table entry (About box) */
{
    static struct notice custom;
    static char tbuf[80], bbuf[900];
    strncpy(tbuf, title, sizeof tbuf - 1); tbuf[sizeof tbuf - 1] = 0;
    strncpy(bbuf, text, sizeof bbuf - 1); bbuf[sizeof bbuf - 1] = 0;
    custom.id = -1; custom.title = tbuf; custom.body = bbuf;
    cur = &custom;
    wrap(cur->body);
    notice_visible = 1;
}

void notice_close(void) { notice_visible = 0; cur = NULL; }

static void rect(UBYTE *fb, int stride, int x, int y, int w, int h, UBYTE c)
{
    for (int j = 0; j < h; j++) memset(fb + (y + j) * stride + x, c, w);
}

void notice_draw(UBYTE *fb, int stride)
{
    if (!notice_visible) return;
    rect(fb, stride, NOTICE_X, NOTICE_Y, NOTICE_W, NOTICE_H, PEN_TEXT);
    rect(fb, stride, NOTICE_X + 2, NOTICE_Y + 2, NOTICE_W - 4, NOTICE_H - 4, PEN_BAR);
    rect(fb, stride, NOTICE_X + 2, NOTICE_Y + 2, NOTICE_W - 4, 26, PEN_BTN);
    rect(fb, stride, NOTICE_X + NOTICE_W / 2 - 50, NOTICE_Y + NOTICE_H - 34, 100, 24, PEN_SEL);
}

void notice_text(void)
{
    if (!notice_visible || !cur) return;
    gfx_colors(PEN_TEXT, PEN_BTN);
    gfx_print(NOTICE_X + 12, NOTICE_Y + 20, T(cur->title));
    gfx_colors(PEN_TEXT, PEN_BAR);
    for (int i = 0; i < nlines; i++) gfx_print(NOTICE_X + 12, NOTICE_Y + 46 + i * 14, lines[i]);
    gfx_colors(PEN_TEXT, PEN_SEL);
    gfx_print_c(NOTICE_X + NOTICE_W / 2, NOTICE_Y + NOTICE_H - 17, T("Continue"));
}

int notice_click(int x, int y)
{
    if (!notice_visible) return 0;
    if (x >= NOTICE_X + NOTICE_W / 2 - 50 && x < NOTICE_X + NOTICE_W / 2 + 50 &&
        y >= NOTICE_Y + NOTICE_H - 34 && y < NOTICE_Y + NOTICE_H - 10)
        notice_close();
    return 1;
}
