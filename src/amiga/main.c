/* Micropolis for AmigaOS: Intuition/cybergraphics (RTG) frontend for the simulation core.
 * Milestone 3: map view with scrolling, animated tiles, sprites, status line, speed control. */
/* sim.h declares a GetSprite() that clashes with graphics.library's */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#include "stack.h"
#include "budget_ui.h"
#include "mapview.h"
#include "notice_ui.h"
#include "eval_ui.h"
#include "graph_ui.h"
#include "gfx.h"
#include "sound.h"
#undef GetSprite
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <proto/cybergraphics.h>
#include <proto/icon.h>
#include <utility/hooks.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>

#include "version.h"
#include "buildinfo.h"
static const char verstag[] __attribute__((used)) = "$VER: Micropolis " VERSION " (" VER_DATE ")";

struct Library *CyberGfxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *AslBase;
struct Library *IconBase;

/* Screen size: 640x480 by default, up to MAX_W x MAX_H (ToolType SIZE=800x600, or MODE=ASK for a mode requester).
 * The tile view fills the screen, so a bigger screen shows more of the city. */
#define MAX_W 800                       /* largest picture: 800x600 (the useful range on RTG cards and emulators) */
#define MAX_H 600
static int scr_w = 640, scr_h = 480;
#define SCR_W scr_w
#define SCR_H scr_h
#define BAR_H 32                       /* status bar on top */
#define TB_W 80                        /* tool palette on the left */
#define MAX_COLS ((MAX_W - TB_W) / 16)
#define MAX_ROWS ((MAX_H - BAR_H) / 16)
static int view_cols = 35, view_rows = 28;
#define VIEW_COLS view_cols            /* tiles visible: (SCR_W - TB_W) / 16 */
#define VIEW_ROWS view_rows


void app_log(const char *msg);
static int debug_log;                  /* DEBUG tooltype / argument: write micropolis.log */
static void logmsg(const char *msg) { app_log(msg); }
void app_log(const char *msg)   /* progress log: when started from the Workbench there is no console */
{
    if (!debug_log) return;
    FILE *f = fopen("PROGDIR:micropolis.log", "a");
    if (f) { fputs(msg, f); fputc(10, f); fclose(f); }
}

/* ---------- preferences: screen mode ----------
 * Default: ask for the RTG screen mode at every start. After choosing one the user can say "always use it"; the
 * choice is saved in ENVARC:Micropolis.prefs (and ENV:), and the menu Options > "Ask for screen mode at start"
 * switches asking on again. */
static int pref_ask = 1;
static ULONG pref_mode = 0xffffffffUL;          /* remembered RTG mode id, picture size and depth */
static int pref_w, pref_h, pref_depth;

static void prefs_load(void)
{
    static const char *const files[] = { "ENV:Micropolis.prefs", "ENVARC:Micropolis.prefs", "PROGDIR:Micropolis.prefs" };
    for (int i = 0; i < 3; i++) {
        FILE *f = fopen(files[i], "r");
        char line[80];
        if (!f) continue;
        while (fgets(line, sizeof line, f)) {
            unsigned long m;
            if (!strncmp(line, "ASK=", 4)) pref_ask = atoi(line + 4);
            else if (sscanf(line, "MODE=%lx", &m) == 1) pref_mode = (ULONG)m;
            else if (!strncmp(line, "SIZE=", 5)) sscanf(line + 5, "%dx%d", &pref_w, &pref_h);
            else if (!strncmp(line, "DEPTH=", 6)) pref_depth = atoi(line + 6);
        }
        fclose(f);
        return;
    }
}

static void prefs_save(void)
{
    static const char *const files[] = { "ENVARC:Micropolis.prefs", "ENV:Micropolis.prefs" };
    for (int i = 0; i < 2; i++) {
        FILE *f = fopen(files[i], "w");
        if (!f) continue;
        fprintf(f, "ASK=%d\nMODE=%lx\nSIZE=%dx%d\nDEPTH=%d\n", pref_ask, (unsigned long)pref_mode, pref_w, pref_h, pref_depth);
        fclose(f);
    }
}

static UBYTE *tiles;                   /* 960 * 256 palette indices */
static UBYTE *objects;                 /* objects.bin */
static UBYTE *fb;                      /* SCR_W x SCR_H chunky frame buffer */
static UBYTE palette[256 * 3];

static int view_x, view_y;             /* top-left tile of the view */
static char status_date[32] = "", status_funds[48] = "", status_msg[96] = "";
extern int sim_loops;
static int dirty = 1;
static int map_dirty = 1;
static int budget_dirty = 1, notice_dirty = 1, eval_dirty = 1, graph_dirty = 1, query_dirty = 1;
static int query_visible;
static char query_f[6][24];                /* zone, density, value, crime, pollution, growth of the queried tile */   /* a panel must be redrawn */
static int demand_r, demand_c, demand_i, demand_dirty = 1;   /* -15..15, from "UISetDemand" */
static void clamp_view(void);

static SimView editor;                 /* the core's tools take a SimView; only tool_state/last_x/y are used */
static int hover_x = -1, hover_y = -1; /* tile under the mouse, -1 = none */
static int dragging;
static int map_drag;
static int sound_ui_ready;                /* set when the main loop runs: UI sounds (tool select, speed) only from then on */
static int shake_frames, shake_bx, shake_by;   /* earthquake: the view jitters by a tile for about a second */
static int dbg_quit_loops;                 /* test option quit=N: leave the game after N seconds (checks the clean shutdown) */
static int dbg_scroll;                  /* test option scroll=N: scroll N tiles (x and y), then stop */
static int debug_show;                  /* test option show=eval|graph: keep messages away so the panel can be photographed */
static const int panning = 0;            /* (right button is the menu button now) */
static int mouse_x = -1, mouse_y = -1;   /* last window position, for edge scrolling */

static const struct { const char *name; short state; } toolbtn[16] = {
    { "Query", queryState },       { "Doze 1", dozeState },       { "Road 10", roadState },
    { "Rail 20", rrState },        { "Wire 5", wireState },       { "Park 10", parkState },
    { "Resid 100", residentialState }, { "Comm 100", commercialState }, { "Indus 100", industrialState },
    { "Fire 500", fireState },     { "Polic 500", policeState }, { "Stadm 5k", stadiumState },
    { "Coal 3k", powerState },     { "Nucl 5k", nuclearState },   { "Sea 3k", seaportState },
    { "Airp 10k", airportState },
};
#define TB_BTN_H ((SCR_H - BAR_H) / 16)

/* ---------- notifications from the simulation core (see ui_core.c) ---------- */
static void copy_braced(char *dst, size_t n, const char *s)
{
    const char *b = strchr(s, '{');
    const char *e = b ? strchr(b, '}') : NULL;
    if (!b || !e) return;
    size_t len = (size_t)(e - b - 1);
    if (len >= n) len = n - 1;
    memcpy(dst, b + 1, len);
    dst[len] = 0;
}

int ui_eval(const char *cmd)
{
    if (!strncmp(cmd, "UISetDate ", 10))         { copy_braced(status_date, sizeof status_date, cmd); dirty = 1; }
    else if (!strncmp(cmd, "UISetFunds ", 11))   { copy_braced(status_funds, sizeof status_funds, cmd); dirty = 1; }
    else if (!strncmp(cmd, "UIShowZoneStatus ", 17)) {
        const char *q = cmd;
        for (int i = 0; i < 6; i++) {
            query_f[i][0] = 0;
            if (!(q = strchr(q, '{'))) break;
            copy_braced(query_f[i], sizeof query_f[i], q);
            if (!(q = strchr(q, '}'))) break;
        }
        query_visible = 1; query_dirty = 1; dirty = 1;
    }
    else if (!strncmp(cmd, "UISetEvaluation ", 16))  { eval_set(cmd); eval_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UISetBudgetValues ", 18)) { budget_set_values(cmd); dirty = 1; }
    else if (!strncmp(cmd, "UIShowBudgetAndWait", 19)) { budget_show(); budget_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UISetDemand ", 12)) {
        int r, c, i;
        if (sscanf(cmd + 12, "%d %d %d", &r, &c, &i) == 3) { demand_r = r; demand_c = c; demand_i = i; demand_dirty = 1; dirty = 1; }
    }
    else if (!strncmp(cmd, "UIShowPicture ", 14))   { if (!debug_show) notice_show(atoi(cmd + 14)); notice_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UIAutoGoto ", 11)) {
        int x, y;
        if (autoGo && sscanf(cmd + 11, "%d %d", &x, &y) == 2) { view_x = x - VIEW_COLS / 2; view_y = y - VIEW_ROWS / 2; clamp_view(); dirty = 1; }
    }
    else if (!strncmp(cmd, "UILoseGame", 10))      { notice_show(200); notice_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UIWinGame", 9))        { notice_show(100); notice_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UIMakeSound ", 12) || !strncmp(cmd, "UIMakeSoundOn ", 14)) {
        const char *q = strrchr(cmd, '"');                    /* last quoted word = sound id */
        if (q) {
            const char *e = q, *b;
            for (b = e - 1; b > cmd && *b != '"'; b--) ;
            if (*b == '"' && e > b + 1 && e - b - 1 < 40) { char id[44]; memcpy(id, b + 1, e - b - 1); id[e - b - 1] = 0; sound_play(id);
                if (debug_log) { char lb[80]; sprintf(lb, "sound: %s", id); logmsg(lb); } }
        }
    }
    else if (!strncmp(cmd, "UISetSpeed ", 11))      { if (sound_ui_ready && UserSoundOn) sound_play("boing"); }
    else if (!strncmp(cmd, "UISetMessage ", 13)) { copy_braced(status_msg, sizeof status_msg, cmd); dirty = 1; }
    return 0;
}
void ui_invalidate(int what) { dirty = 1; }
void ui_flush(void) {}
void ui_update_heads(void) {}

/* ---------- assets ---------- */
static UBYTE *load_file(const char *name, ULONG *size)
{
    BPTR f = Open((CONST_STRPTR)name, MODE_OLDFILE);
    if (!f) return NULL;
    Seek(f, 0, OFFSET_END);
    LONG len = Seek(f, 0, OFFSET_BEGINNING);
    UBYTE *buf = AllocVec(len, MEMF_ANY);
    if (buf && Read(f, buf, len) != len) { FreeVec(buf); buf = NULL; }
    Close(f);
    if (size) *size = len;
    return buf;
}

static int load_assets(void)
{
    UBYTE *pal = load_file("PROGDIR:res/palette.bin", NULL);
    tiles = load_file("PROGDIR:res/tiles.bin", NULL);
    objects = load_file("PROGDIR:res/objects.bin", NULL);
    if (!pal || !tiles || !objects) {
        char b[100];
        sprintf(b, "Assets missing: palette %s tiles %s objects %s (IoErr %ld)", pal ? "ok" : "NO", tiles ? "ok" : "NO", objects ? "ok" : "NO", (long)IoErr());
        logmsg(b);
        printf("Assets fehlen (res/*.bin)\n");
        return 0;
    }
    map_load();
    notice_load();
    memcpy(palette, pal, 256 * 3);
    FreeVec(pal);
    palette[PEN_BAR * 3 + 0] = 0x20; palette[PEN_BAR * 3 + 1] = 0x28; palette[PEN_BAR * 3 + 2] = 0x50;
    palette[PEN_TEXT * 3 + 0] = 0xff; palette[PEN_TEXT * 3 + 1] = 0xff; palette[PEN_TEXT * 3 + 2] = 0xff;
    palette[PEN_SEL * 3 + 0] = 0xb0; palette[PEN_SEL * 3 + 1] = 0x60; palette[PEN_SEL * 3 + 2] = 0x20;
    {   static const UBYTE v[7][3] = { {0xbf,0xbf,0xbf}, {0xff,0xff,0x00}, {0xff,0x7f,0x00}, {0xff,0x00,0x00},
                                       {0x00,0x7f,0x00}, {0x00,0xe6,0x00}, {0x66,0x66,0xe6} };
        for (int i = 0; i < 7; i++) memcpy(&palette[(PEN_V_LGRAY + i) * 3], v[i], 3);
    }
    palette[PEN_BTN * 3 + 0] = 0x40; palette[PEN_BTN * 3 + 1] = 0x48; palette[PEN_BTN * 3 + 2] = 0x70;
    palette[PEN_GREY * 3 + 0] = 0xaa; palette[PEN_GREY * 3 + 1] = 0xaa; palette[PEN_GREY * 3 + 2] = 0xaa;
    return 1;
}

/* ---------- drawing ----------
 * The frame buffer fb always holds the whole picture. Each frame only the tiles whose picture changed
 * (animation, growth, blinking bolt), the areas touched by moving sprites and the cursor, and panels
 * that changed are redrawn in fb and then sent to the RTG screen as small rectangles. On a 68030 with
 * a Zorro II graphics card the bus is the bottleneck, so a full 640x480 blit per frame is far too much. */
struct rect { short x0, y0, x1, y1; };          /* inclusive, frame buffer pixels */

static short shown[MAX_ROWS][MAX_COLS];       /* tile id last drawn in each cell, -1 = unknown */
static UBYTE redraw[MAX_ROWS][MAX_COLS];
static int full_redraw = 1;                     /* start, scroll, panel closed: redraw all cells */
static int toolbar_dirty = 1, first_present = 1, status_dirty = 1;
static int last_view_x = -1, last_view_y = -1;
#define MAX_SPR 48
static struct rect prev_spr[MAX_SPR];
static int n_prev_spr;
static struct rect prev_cursor;
static int have_prev_cursor;
static char last_line1[200], last_line2[200];

static struct { long frames, tiles, bytes, render_us, sim_us, sim_steps; } stats;   /* no doubles: the ieee double library is unreliable */

static void draw_tile(int col, int row, int tile)
{
    const UBYTE *src = tiles + tile * 256;
    UBYTE *dst = fb + (BAR_H + row * 16) * SCR_W + TB_W + col * 16;
    for (int y = 0; y < 16; y++, src += 16, dst += SCR_W)
        memcpy(dst, src, 16);
}

/* picture of a sprite frame: size and pixels (0 = transparent), NULL if unknown */
static const UBYTE *sprite_frame(const SimSprite *s, int *w, int *h)
{
    UWORD n = *(UWORD *)objects;
    const UBYTE *tab = objects + 2;
    if (s->frame == 0) return NULL;
    for (UWORD i = 0; i < n; i++, tab += 8) {
        if (tab[0] != s->type || tab[1] != s->frame - 1) continue;
        *w = tab[2]; *h = tab[3];
        return objects + *(const ULONG *)(tab + 4);
    }
    return NULL;
}

static int sprite_rect(const SimSprite *s, struct rect *r, const UBYTE **px, int *w, int *h)
{
    const UBYTE *p = sprite_frame(s, w, h);
    if (!p) return 0;
    r->x0 = s->x - (view_x << 4) + s->x_offset + TB_W;
    r->y0 = s->y - (view_y << 4) + s->y_offset + BAR_H;
    r->x1 = r->x0 + *w - 1;
    r->y1 = r->y0 + *h - 1;
    if (px) *px = p;
    return 1;
}

static void draw_sprite(const struct rect *r, const UBYTE *px, int w, int h)
{
    for (int y = 0; y < h; y++) {
        int dy = r->y0 + y;
        if (dy < BAR_H || dy >= SCR_H) continue;
        for (int x = 0; x < w; x++) {
            int dx = r->x0 + x;
            UBYTE c = px[y * w + x];
            if (c && dx >= TB_W && dx < SCR_W) fb[dy * SCR_W + dx] = c;
        }
    }
}

static void hline(int x0, int x1, int y, UBYTE c)
{
    if (y < BAR_H || y >= SCR_H) return;
    for (int x = x0; x <= x1; x++) if (x >= TB_W && x < SCR_W) fb[y * SCR_W + x] = c;
}

static void vline(int x, int y0, int y1, UBYTE c)
{
    if (x < TB_W || x >= SCR_W) return;
    for (int y = y0; y <= y1; y++) if (y >= BAR_H && y < SCR_H) fb[y * SCR_W + x] = c;
}

/* area the current tool would build on, at the mouse position */
static int cursor_rect(struct rect *r)
{
    int st = editor.tool_state, size = toolSize[st];
    if (hover_x < 0 || size <= 0) return 0;
    r->x0 = TB_W + (hover_x - toolOffset[st] - view_x) * 16;
    r->y0 = BAR_H + (hover_y - toolOffset[st] - view_y) * 16;
    r->x1 = r->x0 + size * 16 - 1;
    r->y1 = r->y0 + size * 16 - 1;
    return 1;
}

static void draw_cursor(const struct rect *r)
{
    hline(r->x0, r->x1, r->y0, PEN_TEXT); hline(r->x0, r->x1, r->y1, PEN_TEXT);
    vline(r->x0, r->y0, r->y1, PEN_TEXT); vline(r->x1, r->y0, r->y1, PEN_TEXT);
}

static void draw_toolbar(void)
{
    for (int i = 0; i < 16; i++) {
        UBYTE c = toolbtn[i].state == editor.tool_state ? PEN_SEL : PEN_BTN;
        for (int y = 0; y < TB_BTN_H; y++)
            memset(fb + (BAR_H + i * TB_BTN_H + y) * SCR_W, (y == TB_BTN_H - 1) ? PEN_BAR : c, TB_W);
    }
}

/* mark every cell touched by a pixel rectangle */
static void mark_rect(const struct rect *r)
{
    int c0 = (r->x0 - TB_W) >> 4, c1 = (r->x1 - TB_W) >> 4;
    int r0 = (r->y0 - BAR_H) >> 4, r1 = (r->y1 - BAR_H) >> 4;
    if (r->x0 < TB_W) c0 = 0;
    if (r->y0 < BAR_H) r0 = 0;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= VIEW_COLS) c1 = VIEW_COLS - 1;
    if (r1 >= VIEW_ROWS) r1 = VIEW_ROWS - 1;
    for (int y = r0; y <= r1; y++)
        for (int x = c0; x <= c1; x++) redraw[y][x] = 1;
}

/* ---------- panel windows ----------
 * Message, budget, evaluation, history graphs, query box and the overview map are ordinary Intuition windows on the
 * game screen: title bar, drag bar, depth and close gadget; the user can move them (they cannot be resized). The
 * modules (budget_ui.c, eval_ui.c, ...) draw their content into the scratch buffer at their own coordinates; this file
 * blits that area into the window. A panel is open exactly while its *visible flag is set: modules and commands set
 * and clear the flags, sync_panels() opens, redraws and closes the windows accordingly. */
static UBYTE *scratch;                      /* MAX_W x MAX_H chunky buffer the panel modules draw into */
#define SCRATCH_STRIDE MAX_W

struct panel {
    const char *title;
    int *visible, *dirty;
    int ox, oy, w, h;                        /* content area in scratch coordinates (= the module's coordinates) */
    void (*draw)(void);                      /* draws the content into scratch */
    int (*click)(int x, int y);              /* left click, in scratch coordinates; may be NULL */
    void (*close)(void);                     /* close gadget / Esc: closes the panel (budget: Continue) */
    int placement;                           /* 0 centre, 1 top right, 2 bottom right */
    int wants_move;                          /* also wants IDCMP_MOUSEMOVE (the map drags) */
    struct Window *win;
};
enum { P_NOTICE, P_BUDGET, P_EVAL, P_GRAPH, P_QUERY, P_MAP, NPANELS };
#define QUERY_X 0
#define QUERY_Y 0
#define QUERY_W 236
#define QUERY_H 112
#define MAPWIN_W (MV_W + 2)
#define MAPWIN_H (MV_H + 2)
static int map_panel_dirty = 1, map_view_x = -1, map_view_y = -1;

static void draw_notice(void) { gfx_target(scratch, SCRATCH_STRIDE); notice_draw(scratch, SCRATCH_STRIDE); notice_text(); }
static void draw_budget(void) { gfx_target(scratch, SCRATCH_STRIDE); budget_draw(scratch, SCRATCH_STRIDE); budget_text(); }
static void draw_eval(void)   { gfx_target(scratch, SCRATCH_STRIDE); eval_draw(scratch, SCRATCH_STRIDE); }
static void draw_graph(void)  { gfx_target(scratch, SCRATCH_STRIDE); graph_draw(scratch, SCRATCH_STRIDE); }
static void draw_map(void)    { map_blit(scratch, SCRATCH_STRIDE, 1, 1, view_x, view_y, VIEW_COLS, VIEW_ROWS); }
static void draw_query(void)
{
    static const char *const label[6] = { "Zone", "Density", "Value", "Crime", "Pollution", "Growth" };
    char b[48];
    for (int y = 0; y < QUERY_H; y++) memset(scratch + (QUERY_Y + y) * SCRATCH_STRIDE + QUERY_X, PEN_BAR, QUERY_W);
    gfx_target(scratch, SCRATCH_STRIDE);
    gfx_colors(PEN_TEXT, PEN_BAR);
    gfx_print(QUERY_X + 8, QUERY_Y + 18, "Query zone status");
    for (int i = 0; i < 6; i++) { sprintf(b, "%-10s %s", label[i], query_f[i]); gfx_print(QUERY_X + 8, QUERY_Y + 36 + i * 13, b); }
}
static void close_query(void) { query_visible = 0; }
static void close_notice(void) { notice_close(); }
static void close_map(void) { map_visible = 0; }
static struct panel panels[NPANELS] = {
    { "Message",             &notice_visible, &notice_dirty, NOTICE_X, NOTICE_Y, NOTICE_W, NOTICE_H, draw_notice, notice_click, close_notice, 0, 0, NULL },
    { "Budget",              &budget_visible, &budget_dirty, BUDGET_X, BUDGET_Y, BUDGET_W, BUDGET_H, draw_budget, budget_click, budget_key_close, 0, 0, NULL },
    { "City Evaluation",     &eval_visible,   &eval_dirty,   EVAL_X, EVAL_Y, EVAL_W, EVAL_H, draw_eval, eval_click, eval_close, 0, 0, NULL },
    { "History",             &graph_visible,  &graph_dirty,  GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, draw_graph, graph_click, graph_close, 0, 0, NULL },
    { "Query",               &query_visible,  &query_dirty,  QUERY_X, QUERY_Y, QUERY_W, QUERY_H, draw_query, NULL, close_query, 1, 0, NULL },
    { "Overview map",        &map_visible,    &map_panel_dirty, 0, 0, MAPWIN_W, MAPWIN_H, draw_map, NULL, close_map, 2, 1, NULL },
};

static void open_info_panel(int which)       /* menu / key: show or hide a window */
{
    *panels[which].visible = !*panels[which].visible;
    if (*panels[which].visible) *panels[which].dirty = 1;
}

static int panels_open(void)
{
    int n = 0;
    for (int i = 0; i < NPANELS; i++) n += panels[i].win != NULL;
    return n;
}

static int true_color;                     /* screen deeper than 8 bit: blit through a colour table */
static ULONG ctable[256];                  /* 0x00RRGGBB for each palette index */

/* copy a rectangle of a chunky buffer into a window's rastport (palette screens: LUT8, deeper screens: colour table) */
static void blit_buf(UBYTE *buf, int stride, int sx, int sy, struct RastPort *rp, int dx, int dy, int w, int h)
{
    if (true_color)
        WriteLUTPixelArray(buf + sy * stride + sx, 0, 0, stride, rp, ctable, dx, dy, w, h, CTABFMT_XRGB8);
    else
        WritePixelArray(buf, sx, sy, stride, rp, dx, dy, w, h, RECTFMT_LUT8);
    stats.bytes += (long)w * h;
}

static void blit(struct RastPort *rp, int x, int y, int w, int h)       /* main window: same coordinates */
{
    blit_buf(fb, SCR_W, x, y, rp, x, y, w, h);
}

/* ---- panel windows: open, close, redraw ---- */
static struct Screen *g_scr;
static struct Window *main_window_ptr;
static int map_data_loops = -1000;
static struct MsgPort *idcmp_port;           /* one message port for all windows */
static struct Menu *g_menu;                  /* the menu strip is attached to every window */
static char screen_title[120] = "Micropolis";

static void close_window_safely(struct Window *w)
{
    struct IntuiMessage *m;
    struct Node *succ;
    if (g_menu) ClearMenuStrip(w);
    Forbid();
    for (m = (struct IntuiMessage *)w->UserPort->mp_MsgList.lh_Head; (succ = m->ExecMessage.mn_Node.ln_Succ);
         m = (struct IntuiMessage *)succ) {
        if (m->IDCMPWindow == w) { Remove((struct Node *)m); ReplyMsg((struct Message *)m); }
    }
    w->UserPort = NULL;
    ModifyIDCMP(w, 0);
    Permit();
    CloseWindow(w);
}

static int panel_index(const struct Window *w)
{
    for (int i = 0; i < NPANELS; i++) if (panels[i].win == w) return i;
    return -1;
}

static void panel_render(int i)
{
    struct panel *p = &panels[i];
    struct Window *w = p->win;
    if (!w) return;
    if (i == P_MAP) {                                    /* keep the overlay data fresh */
        if (map_dirty || sim_loops - map_data_loops >= 16) { map_refresh(); map_dirty = 0; map_data_loops = sim_loops; }
        map_view_x = view_x; map_view_y = view_y;
    }
    p->draw();
    blit_buf(scratch, SCRATCH_STRIDE, p->ox, p->oy, w->RPort, w->BorderLeft, w->BorderTop, p->w, p->h);
}

static void panel_open(int i)
{
    struct panel *p = &panels[i];
    int sw = g_scr->Width, sh = g_scr->Height, bar = g_scr->BarHeight + 1;
    int x, y;
    if (p->placement == 1) { x = sw - p->w - 24; y = bar + 40; }
    else if (p->placement == 2) { x = sw - p->w - 24; y = sh - p->h - 40; }
    else { x = (sw - p->w - 8) / 2; y = bar + (sh - bar - p->h - 24) / 2; }
    if (x < 0) x = 0;
    if (y < bar) y = bar;
    p->win = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)g_scr, WA_Left, x, WA_Top, y,
                            WA_InnerWidth, p->w, WA_InnerHeight, p->h, WA_Title, (ULONG)p->title,
                            WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
                            WA_SimpleRefresh, TRUE, WA_ReportMouse, p->wants_move ? TRUE : FALSE,
                            WA_IDCMP, 0, TAG_DONE);
    if (!p->win) { *p->visible = 0; return; }
    p->win->UserPort = idcmp_port;
    ModifyIDCMP(p->win, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                        (p->wants_move ? IDCMP_MOUSEMOVE : 0));
    if (g_menu) SetMenuStrip(p->win, g_menu);
    SetWindowTitles(p->win, (UBYTE *)-1, (UBYTE *)screen_title);
    *p->dirty = 1;
}

static void panel_close(int i)
{
    struct panel *p = &panels[i];
    if (!p->win) return;
    close_window_safely(p->win);
    p->win = NULL;
}

/* opens, closes and redraws the panel windows according to the flags; called every loop */
static void sync_panels(void)
{
    for (int i = 0; i < NPANELS; i++) {
        struct panel *p = &panels[i];
        if (*p->visible && !p->win) panel_open(i);
        else if (!*p->visible && p->win) panel_close(i);
        if (i == P_MAP && p->win && (view_x != map_view_x || view_y != map_view_y || map_dirty || sim_loops - map_data_loops >= 16))
            *p->dirty = 1;                               /* the view frame moved, or the data is old */
        if (p->win && *p->dirty) { *p->dirty = 0; panel_render(i); }
    }
}

static void set_screen_title(const char *t)
{
    if (!strcmp(t, screen_title)) return;
    strncpy(screen_title, t, sizeof screen_title - 1);
    screen_title[sizeof screen_title - 1] = 0;
    if (g_scr) {
        if (main_window_ptr) SetWindowTitles(main_window_ptr, (UBYTE *)-1, (UBYTE *)screen_title);
        for (int i = 0; i < NPANELS; i++) if (panels[i].win) SetWindowTitles(panels[i].win, (UBYTE *)-1, (UBYTE *)screen_title);
    }
}

static struct timeval t_a;
static long elapsed_us(void)
{
    struct timeval t_b;
    gettimeofday(&t_b, NULL);
    return (long)(t_b.tv_sec - t_a.tv_sec) * 1000000L + (long)(t_b.tv_usec - t_a.tv_usec);
}

/* residential / commercial / industrial demand bars in the status bar (what the city needs) */
#define DEM_X 520
#define DEM_Y 0
#define DEM_W 100
#define DEM_H 32
static void draw_demand(void)
{
    static const UBYTE pen[3] = { PEN_V_LGREEN, PEN_V_LBLUE, PEN_V_YELLOW };
    int v[3] = { demand_r, demand_c, demand_i };
    for (int y = 0; y < DEM_H; y++) memset(fb + (DEM_Y + y) * SCR_W + DEM_X, PEN_BAR, DEM_W);
    for (int k = 0; k < 3; k++) {
        int h = (v[k] > 15 ? 15 : v[k] < -15 ? -15 : v[k]) * 9 / 15;     /* +-9 pixels */
        int x0 = DEM_X + 4 + k * 36, y0 = DEM_Y + 12;          /* zero line at y0 */
        for (int x = 0; x < 20; x++) fb[(y0) * SCR_W + x0 + x] = PEN_TEXT;
        if (h > 0) for (int y = 1; y <= h; y++) memset(fb + (y0 - y) * SCR_W + x0, pen[k], 20);
        else for (int y = 1; y <= -h; y++) memset(fb + (y0 + y) * SCR_W + x0, pen[k], 20);
    }
}

/* Small scroll steps: move the picture inside the RTG screen (graphics.library blit, no bus traffic from the CPU)
 * and redraw only the newly exposed row/column of tiles. Only used while no panel and no overview map covers the
 * view, otherwise the whole view is redrawn (see render_frame). */
static void scroll_by(struct RastPort *rp, int dx, int dy)
{
    static short old[MAX_ROWS][MAX_COLS];
    int sx = TB_W, sy = BAR_H, w = VIEW_COLS * 16, h = VIEW_ROWS * 16, tx = sx, ty = sy;
    if (dx > 0) { sx += dx * 16; w -= dx * 16; } else if (dx < 0) { tx -= dx * 16; w += dx * 16; }
    if (dy > 0) { sy += dy * 16; h -= dy * 16; } else if (dy < 0) { ty -= dy * 16; h += dy * 16; }
    ClipBlit(rp, sx, sy, rp, tx, ty, w, h, 0xC0);
    memcpy(old, shown, sizeof old);                       /* what each tile cell showed before */
    for (int r = 0; r < VIEW_ROWS; r++)
        for (int c = 0; c < VIEW_COLS; c++) {
            int or_ = r + dy, oc = c + dx;
            shown[r][c] = (or_ >= 0 && or_ < VIEW_ROWS && oc >= 0 && oc < VIEW_COLS) ? old[or_][oc] : -1;
        }
    for (int i = 0; i < n_prev_spr; i++) { prev_spr[i].x0 -= dx * 16; prev_spr[i].x1 -= dx * 16; prev_spr[i].y0 -= dy * 16; prev_spr[i].y1 -= dy * 16; }
    prev_cursor.x0 -= dx * 16; prev_cursor.x1 -= dx * 16; prev_cursor.y0 -= dy * 16; prev_cursor.y1 -= dy * 16;
}

static void render_frame(struct Window *win)
{
    struct RastPort *rp = win->RPort;
    int blink = flagBlink > 0;
    struct rect cur_spr[MAX_SPR], cur;
    const UBYTE *spr_px[MAX_SPR];
    int spr_w[MAX_SPR], spr_h[MAX_SPR], n_spr = 0;
    int have_cursor;

    gettimeofday(&t_a, NULL);
    if (view_x != last_view_x || view_y != last_view_y) {
        int ddx = view_x - last_view_x, ddy = view_y - last_view_y;
        if (!full_redraw && !first_present && last_view_x >= 0 && panels_open() == 0 &&
            ddx >= -6 && ddx <= 6 && ddy >= -6 && ddy <= 6)
            scroll_by(rp, ddx, ddy);
        else
            full_redraw = 1;
        last_view_x = view_x; last_view_y = view_y;
    }
    if (full_redraw) {
        memset(redraw, 1, sizeof redraw);
        memset(shown, 0xff, sizeof shown);
        have_prev_cursor = 0; n_prev_spr = 0;
        full_redraw = 0;
    } else {
        memset(redraw, 0, sizeof redraw);
    }

    /* which cells changed? */
    for (int row = 0; row < VIEW_ROWS; row++) {
        for (int col = 0; col < VIEW_COLS; col++) {
            int mx = view_x + col, my = view_y + row, tile = 0;
            if (mx < WORLD_X && my < WORLD_Y) {
                tile = (unsigned short)Map[mx][my];
                if ((tile & LOMASK) >= TILE_COUNT) tile -= TILE_COUNT;
                if (blink && (tile & ZONEBIT) && !(tile & PWRBIT)) tile = LIGHTNINGBOLT;
                else tile &= LOMASK;
            }
            if (tile != shown[row][col]) { redraw[row][col] = 1; shown[row][col] = tile; }
        }
    }

    /* moving things: redraw the cells under their old and new positions */
    for (SimSprite *sp = sim->sprite; sp && n_spr < MAX_SPR; sp = sp->next) {
        if (sprite_rect(sp, &cur_spr[n_spr], &spr_px[n_spr], &spr_w[n_spr], &spr_h[n_spr])) {
            mark_rect(&cur_spr[n_spr]);
            n_spr++;
        }
    }
    for (int i = 0; i < n_prev_spr; i++) mark_rect(&prev_spr[i]);
    have_cursor = cursor_rect(&cur);
    if (have_cursor) mark_rect(&cur);
    if (have_prev_cursor) mark_rect(&prev_cursor);

    /* draw the tiles that changed */
    for (int row = 0; row < VIEW_ROWS; row++) {
        for (int col = 0; col < VIEW_COLS; col++) {
            if (!redraw[row][col]) continue;
            draw_tile(col, row, shown[row][col]);
            stats.tiles++;
        }
    }
    for (int i = 0; i < n_spr; i++) draw_sprite(&cur_spr[i], spr_px[i], spr_w[i], spr_h[i]);
    if (have_cursor) draw_cursor(&cur);
    for (int i = 0; i < n_spr; i++) prev_spr[i] = cur_spr[i];
    n_prev_spr = n_spr;
    prev_cursor = cur; have_prev_cursor = have_cursor;

    gfx_target(fb, SCR_W);
    if (toolbar_dirty) {
        draw_toolbar();
        gfx_colors(PEN_TEXT, PEN_BTN);
        for (int i = 0; i < 16; i++) {
            gfx_colors(PEN_TEXT, toolbtn[i].state == editor.tool_state ? PEN_SEL : PEN_BTN);
            gfx_print(4, BAR_H + i * TB_BTN_H + TB_BTN_H / 2 + 4, toolbtn[i].name);
        }
    }
    {   /* status lines (top left) */
        char line[200];
        gfx_colors(PEN_TEXT, PEN_BAR);
        sprintf(line, " %-9s %-21s Pop %-7ld Speed %d%s", status_date, status_funds, (long)CityPop, (int)SimSpeed,
                SimSpeed ? "" : " (Pause)");
        if (strcmp(line, last_line1)) { gfx_print(4, 12, line); strcpy(last_line1, line); status_dirty = 1; }
        sprintf(line, " %-58.58s", status_msg);
        if (strcmp(line, last_line2)) { gfx_print(4, 27, line); strcpy(last_line2, line); status_dirty = 1; }
    }
    if (demand_dirty) {
        draw_demand();
        gfx_colors(PEN_TEXT, PEN_BAR);
        gfx_print(DEM_X + 10, DEM_Y + 30, "R"); gfx_print(DEM_X + 46, DEM_Y + 30, "C"); gfx_print(DEM_X + 82, DEM_Y + 30, "I");
    }
    stats.render_us += elapsed_us();

    /* send the changes to the screen */
    if (first_present) {
        blit(rp, 0, 0, SCR_W, SCR_H);
        first_present = 0;
    } else {
        for (int row = 0; row < VIEW_ROWS; row++) {
            int col = 0;
            while (col < VIEW_COLS) {
                int c0;
                if (!redraw[row][col]) { col++; continue; }
                c0 = col;
                while (col < VIEW_COLS && redraw[row][col]) col++;
                blit(rp, TB_W + c0 * 16, BAR_H + row * 16, (col - c0) * 16, 16);
            }
        }
        if (toolbar_dirty) blit(rp, 0, BAR_H, TB_W, SCR_H - BAR_H);
        if (demand_dirty) blit(rp, DEM_X, DEM_Y, DEM_W, DEM_H);
        if (status_dirty) blit(rp, 0, 0, DEM_X, BAR_H);
    }

    toolbar_dirty = 0; status_dirty = 0; demand_dirty = 0;

    if (++stats.frames >= 200) {                     /* performance log for tuning on slow machines */
        char b[160];
        sprintf(b, "perf: %ld frames, render %ld us/frame, %ld tiles/frame, %ld KB blit/frame; %ld sim steps, %ld us/step",
                stats.frames, stats.render_us / stats.frames, stats.tiles / stats.frames,
                stats.bytes / stats.frames / 1024, stats.sim_steps, stats.sim_steps ? stats.sim_us / stats.sim_steps : 0L);
        logmsg(b);
        memset(&stats, 0, sizeof stats);
    }
}

/* ---------- menus and commands ---------- */
enum { CMD_NEW = 1, CMD_LOAD, CMD_SAVE, CMD_SAVEAS, CMD_QUIT,
       CMD_PAUSE = 10, CMD_SLOW, CMD_MEDIUM, CMD_FAST,
       CMD_AUTOBULLDOZE = 20, CMD_AUTOBUDGET, CMD_DISASTERS, CMD_SOUND, CMD_ASKMODE,
       CMD_ABOUT = 44, CMD_SCEN = 200, CMD_BUDGET = 40, CMD_MAP = 41, CMD_EVAL = 42, CMD_GRAPHS = 43, CMD_MAPMODE = 100,
       CMD_FIRE = 30, CMD_FLOOD, CMD_TORNADO, CMD_QUAKE, CMD_MONSTER, CMD_MELTDOWN };

static struct NewMenu newmenu[64];                      /* the menus have about 48 entries */
static int nmenu;
static void mi(UBYTE type, const char *label, const char *key, UWORD flags, int id)
{
    if (nmenu >= (int)(sizeof newmenu / sizeof newmenu[0]) - 1) return;     /* never overrun the table */
    struct NewMenu *n = &newmenu[nmenu++];
    n->nm_Type = type; n->nm_Label = (STRPTR)label; n->nm_CommKey = (STRPTR)key;
    n->nm_Flags = flags; n->nm_MutualExclude = 0; n->nm_UserData = (APTR)(long)id;
}

static struct Menu *make_menus(struct Screen *scr, APTR *vi_out)
{
    APTR vi = GetVisualInfoA(scr, NULL);
    struct Menu *menu;
    nmenu = 0;
    mi(NM_TITLE, "Project", NULL, 0, 0);
    mi(NM_ITEM, "About Micropolis...", NULL, 0, CMD_ABOUT);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    mi(NM_ITEM, "New City", "N", 0, CMD_NEW);
    mi(NM_ITEM, "Load City...", "L", 0, CMD_LOAD);
    mi(NM_ITEM, "Save City", "S", 0, CMD_SAVE);
    mi(NM_ITEM, "Save City As...", NULL, 0, CMD_SAVEAS);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    mi(NM_ITEM, "Quit", "Q", 0, CMD_QUIT);
    mi(NM_TITLE, "Scenarios", NULL, 0, 0);
    {   static const char *const scen[8] = { "Dullsville 1900", "San Francisco 1906", "Hamburg 1944", "Bern 1965",
                                              "Tokyo 1957", "Detroit 1972", "Boston 2010", "Rio de Janeiro 2047" };
        for (int i = 0; i < 8; i++) mi(NM_ITEM, scen[i], NULL, 0, CMD_SCEN + i + 1);
    }
    mi(NM_TITLE, "Speed", NULL, 0, 0);
    mi(NM_ITEM, "Pause", "0", 0, CMD_PAUSE);
    mi(NM_ITEM, "Slow", "1", 0, CMD_SLOW);
    mi(NM_ITEM, "Medium", "2", 0, CMD_MEDIUM);
    mi(NM_ITEM, "Fast", "3", 0, CMD_FAST);
    mi(NM_TITLE, "Options", NULL, 0, 0);
    mi(NM_ITEM, "Auto Bulldoze", NULL, CHECKIT | MENUTOGGLE | (autoBulldoze ? CHECKED : 0), CMD_AUTOBULLDOZE);
    mi(NM_ITEM, "Auto Budget", NULL, CHECKIT | MENUTOGGLE | (autoBudget ? CHECKED : 0), CMD_AUTOBUDGET);
    mi(NM_ITEM, "Disasters", NULL, CHECKIT | MENUTOGGLE | (NoDisasters ? 0 : CHECKED), CMD_DISASTERS);
    mi(NM_ITEM, "Sound", NULL, CHECKIT | MENUTOGGLE | (UserSoundOn ? CHECKED : 0), CMD_SOUND);
    mi(NM_ITEM, "Ask for screen mode at start", NULL, CHECKIT | MENUTOGGLE | (pref_ask ? CHECKED : 0), CMD_ASKMODE);
    mi(NM_TITLE, "Windows", NULL, 0, 0);
    mi(NM_ITEM, "Budget...", "B", 0, CMD_BUDGET);
    mi(NM_ITEM, "Evaluation...", "E", 0, CMD_EVAL);
    mi(NM_ITEM, "Graphs...", "G", 0, CMD_GRAPHS);
    mi(NM_TITLE, "Map", NULL, 0, 0);
    mi(NM_ITEM, "Map window on/off", "M", 0, CMD_MAP);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    for (int i = 0; i < MAP_MODES; i++) mi(NM_ITEM, map_names[i], NULL, 0, CMD_MAPMODE + i);
    mi(NM_TITLE, "Disasters", NULL, 0, 0);
    mi(NM_ITEM, "Fire", NULL, 0, CMD_FIRE);
    mi(NM_ITEM, "Flood", NULL, 0, CMD_FLOOD);
    mi(NM_ITEM, "Tornado", NULL, 0, CMD_TORNADO);
    mi(NM_ITEM, "Earthquake", NULL, 0, CMD_QUAKE);
    mi(NM_ITEM, "Monster", NULL, 0, CMD_MONSTER);
    mi(NM_ITEM, "Meltdown", NULL, 0, CMD_MELTDOWN);
    mi(NM_END, NULL, NULL, 0, 0);
    *vi_out = vi;
    if (!vi) return NULL;
    menu = CreateMenusA(newmenu, NULL);
    if (menu && !LayoutMenusA(menu, vi, NULL)) { FreeMenus(menu); menu = NULL; }
    return menu;
}

/* file requester (asl.library); returns 1 and a full path in out, 0 if cancelled */
static int ask_file(struct Window *win, int save, char *out, size_t n)
{
    int ok = 0;
    if (!AslBase) return 0;
    struct FileRequester *fr = AllocAslRequestTags(ASL_FileRequest,
        ASLFR_Window, (ULONG)win, ASLFR_TitleText, (ULONG)(save ? "Save City" : "Load City"),
        ASLFR_InitialDrawer, (ULONG)"PROGDIR:cities", ASLFR_InitialPattern, (ULONG)"#?.cty",
        ASLFR_DoPatterns, TRUE, ASLFR_DoSaveMode, save, ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (fr) {
        if (AslRequest(fr, NULL)) {
            strncpy(out, (char *)fr->fr_Drawer, n - 1); out[n - 1] = 0;
            if (AddPart((STRPTR)out, fr->fr_File, n)) ok = 1;
        }
        FreeAslRequest(fr);
    }
    return ok;
}

static void recenter(void)
{
    view_x = WORLD_X / 2 - VIEW_COLS / 2;
    view_y = WORLD_Y / 2 - VIEW_ROWS / 2;
}

/* ---------- About box: version, build, machine - to tell which variant is running ---------- */
static int scr_depth_shown, scr_mode_shown;       /* filled in at startup */

static void show_about(void)
{
    char txt[800], cpu[40] = "68000", fpu[24] = "no FPU", modeline[60], snd[60];
    sound_status(snd);
    if (!UserSoundOn) strcpy(snd, "Sound: switched off (Options menu)");
    sprintf(modeline, "Screen 0x%08lx, %dx%d, %d bit", (unsigned long)scr_mode_shown, scr_w, scr_h, scr_depth_shown);
    UWORD af = SysBase->AttnFlags;
    if (af & AFF_68010) strcpy(cpu, "68010");
    if (af & AFF_68020) strcpy(cpu, "68020");
    if (af & AFF_68030) strcpy(cpu, "68030");
    if (af & AFF_68040) strcpy(cpu, "68040");
    if (af & 0x80) strcpy(cpu, "68060");                 /* AFF_68060 */
    if (af & (AFF_68881 | AFF_68882)) strcpy(fpu, (af & AFF_68882) ? "68882" : "68881");
    if (af & AFF_FPU40) strcpy(fpu, "FPU");
    sprintf(txt,
        "Micropolis for AmigaOS 3\n"
        "Version %s (%s)\n"
        "Build  %s  %s\n"
        "\n"
        "CPU %s, %s\n"
        "%s\n"
        "Memory: %ld KB Chip, %ld KB Fast free\n"
        "%s\n"
        "\n"
        "Simulation and graphics from Micropolis, the open-source SimCity (GPL v3, (c) Electronic Arts). "
        "Original design Will Wright, Unix version Don Hopkins.\n"
        "github.com/joergsf4/micropolis-amiga",
        VERSION, VER_DATE, BUILD_TIME, BUILD_REV, cpu, fpu,
        modeline,
        (long)(AvailMem(MEMF_CHIP) / 1024), (long)(AvailMem(MEMF_FAST) / 1024), snd);
    notice_show_text("About Micropolis", txt);
    notice_dirty = 1;
}

/* returns 0 to quit */
static int do_command(int id, struct Window *win, struct MenuItem *item)
{
    char path[256];
    int checked = item && (item->Flags & CHECKED);
    if (id >= CMD_MAPMODE && id < CMD_MAPMODE + MAP_MODES) {
        map_mode = id - CMD_MAPMODE; map_visible = 1; map_dirty = 1; map_panel_dirty = 1;
        sprintf(status_msg, "Map: %s", map_names[map_mode]);
        return 1;
    }
    if (id > CMD_SCEN && id <= CMD_SCEN + 8) {            /* scenario: load it and show its story, as the original does */
        LoadScenario((short)(id - CMD_SCEN));
        recenter();
        notice_show(id - CMD_SCEN); notice_dirty = 1;
        return 1;
    }
    switch (id) {
    case CMD_ABOUT: show_about(); break;
    case CMD_NEW:
        GenerateNewCity(); setSpeed(3); recenter();
        sprintf(status_msg, "New city generated."); break;
    case CMD_LOAD:
        if (ask_file(win, 0, path, sizeof path)) {
            if (LoadCity(path)) { recenter(); sprintf(status_msg, "Loaded %s", path); }
            else sprintf(status_msg, "Could not load %s", path);
        }
        break;
    case CMD_SAVE:
        if (CityFileName) { SaveCityAs(CityFileName); sprintf(status_msg, "Saved %s", CityFileName); break; }
        /* no name yet: fall through to "Save As" */
    case CMD_SAVEAS:
        if (ask_file(win, 1, path, sizeof path)) {
            SaveCityAs(path); sprintf(status_msg, "Saved %s", path);
        }
        break;
    case CMD_QUIT: return 0;
    case CMD_PAUSE: setSpeed(0); break;
    case CMD_SLOW: setSpeed(1); break;
    case CMD_MEDIUM: setSpeed(2); break;
    case CMD_FAST: setSpeed(3); break;
    case CMD_AUTOBULLDOZE: autoBulldoze = checked; break;
    case CMD_AUTOBUDGET: autoBudget = checked; break;
    case CMD_DISASTERS: NoDisasters = !checked; break;
    case CMD_SOUND: UserSoundOn = checked; break;
    case CMD_ASKMODE:
        pref_ask = checked;
        if (!pref_ask) { pref_mode = (ULONG)scr_mode_shown; pref_w = scr_w; pref_h = scr_h; pref_depth = scr_depth_shown; }   /* keep using the current mode */
        prefs_save();
        sprintf(status_msg, pref_ask ? "The screen mode will be asked at the next start." : "This screen mode is used from now on.");
        break;
    case CMD_BUDGET: DoBudgetFromMenu(); break;
    case CMD_EVAL: open_info_panel(P_EVAL); break;
    case CMD_GRAPHS: open_info_panel(P_GRAPH); break;
    case CMD_MAP: map_visible = !map_visible; map_dirty = 1; break;
    case CMD_FIRE: MakeFire(); break;
    case CMD_FLOOD: MakeFlood(); break;
    case CMD_TORNADO: MakeTornado(); break;
    case CMD_QUAKE: MakeEarthquake(); break;
    case CMD_MONSTER: MakeMonster(); break;
    case CMD_MELTDOWN: MakeMeltdown(); break;
    }
    return 1;
}

static void clamp_view(void)
{
    if (view_x > WORLD_X - VIEW_COLS) view_x = WORLD_X - VIEW_COLS;
    if (view_y > WORLD_Y - VIEW_ROWS) view_y = WORLD_Y - VIEW_ROWS;
    if (view_x < 0) view_x = 0;
    if (view_y < 0) view_y = 0;
}

/* ---------- errors, tooltypes, screen mode ---------- */
static void fail(const char *fmt, ...)
{
    char msg[300];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);
    printf("Micropolis: %s\n", msg);
    logmsg(msg);
    if (IntuitionBase) {
        struct EasyStruct es;
        es.es_StructSize = sizeof(struct EasyStruct); es.es_Flags = 0;
        es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)"%s"; es.es_GadgetFormat = (CONST_STRPTR)"OK";
        {   ULONG args[1];
            args[0] = (ULONG)msg;
            EasyRequestArgs(NULL, &es, NULL, (APTR)args);
        }
    }
}

static int req_w = 640, req_h = 480;      /* wanted screen size: ToolType SIZE=WxH */
static int ask_mode_flag;                 /* argument ASK / ToolType MODE=ASK: ask now, whatever the saved setting says */
static int size_forced;                   /* SIZE= given: use exactly that size, do not ask */

/* ToolTypes of the program's icon (PROGDIR:Micropolis.info); CLI arguments work as well */
static char tt_city[200];
static int  tt_debug;
static int  force_depth;                 /* depth=NN argument for testing other screen depths */

static void read_tooltypes(int argc, char **argv)
{
    IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 37);
    if (IconBase) {
        struct DiskObject *dobj = GetDiskObject((CONST_STRPTR)"PROGDIR:Micropolis");
        if (dobj) {
            STRPTR v;
            if (dobj->do_ToolTypes) {
                if ((v = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"CITY"))) { strncpy(tt_city, (char *)v, sizeof tt_city - 1); }
                if (FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"DEBUG")) tt_debug = 1;
                if ((v = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"SIZE"))) { sscanf((char *)v, "%dx%d", &req_w, &req_h); size_forced = 1; }
                if ((v = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"MODE")) && !strcasecmp((char *)v, "ASK")) ask_mode_flag = 1;
            }
            FreeDiskObject(dobj);
        }
    }
    for (int i = 1; i < argc; i++) {                     /* argc == 0 (Workbench): argv is not a string vector */
        if (!strcasecmp(argv[i], "debug")) tt_debug = 1;
        else if (!strncasecmp(argv[i], "depth=", 6)) force_depth = atoi(argv[i] + 6);
        else if (!strncasecmp(argv[i], "size=", 5)) { sscanf(argv[i] + 5, "%dx%d", &req_w, &req_h); size_forced = 1; }
        else if (!strcasecmp(argv[i], "ask")) ask_mode_flag = 1;
    }
}

/* Sensible modes only: RTG modes between 640x480 and 800x600 with a 4:3 / 5:4 shape and at least 8 bit. */
static int relax_filter;                  /* the card has no mode that passes the strict rules: accept any RTG mode >= 640x480 */

static int mode_ok(ULONG id)
{
    long w, h, d;
    if (id == INVALID_ID || !IsCyberModeID(id)) return 0;
    w = (long)GetCyberIDAttr(CYBRIDATTR_WIDTH, id); h = (long)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
    d = (long)GetCyberIDAttr(CYBRIDATTR_DEPTH, id);
    if (w < 640 || h < 480 || d < 8) return 0;
    if (relax_filter) return 1;
    if (w > MAX_W || h > MAX_H) return 0;
    return w * 10 >= h * 12 && w * 10 <= h * 14;          /* aspect ratio 1.2 .. 1.4 */
}

/* does the card offer any mode that passes the strict rules? (the names of the modes do not matter, only size and depth) */
static void check_filter(void)
{
    ULONG m = INVALID_ID;
    int n = 0;
    relax_filter = 0;
    while ((m = NextDisplayInfo(m)) != INVALID_ID) n += mode_ok(m);
    if (!n) { relax_filter = 1; logmsg("no mode passes the strict screen mode rules: accepting any RTG mode >= 640x480"); }
}

/* asl filter hook: called with A0 = hook, A2 = requester, A1 = display mode id; TRUE = show the mode */
static ULONG mode_filter(register struct Hook *h __asm("a0"), register APTR req __asm("a2"), register ULONG id __asm("a1"))
{
    return mode_ok(id) ? 1 : 0;
}

/* screen mode requester (asl.library): returns the chosen RTG mode id, its size and depth; INVALID_ID if cancelled */
static ULONG ask_mode(int *w, int *h, int *depth)
{
    ULONG id = INVALID_ID;
    struct ScreenModeRequester *sr;
    static struct Hook filter_hook;
    if (!AslBase) return INVALID_ID;
    filter_hook.h_Entry = (ULONG (*)())mode_filter;
    sr = (struct ScreenModeRequester *)AllocAslRequestTags(ASL_ScreenModeRequest,
            ASLSM_TitleText, (ULONG)"Micropolis: choose an RTG screen mode (640x480 to 800x600)",
            ASLSM_MinWidth, 640, ASLSM_MinHeight, 480, ASLSM_MinDepth, 8,
            ASLSM_InitialDisplayID, (pref_mode != 0xffffffffUL) ? pref_mode : 0,
            ASLSM_FilterFunc, (ULONG)&filter_hook, TAG_DONE);
    if (sr) {
        for (int tries = 0; tries < 5; tries++) {
            if (!AslRequest(sr, NULL)) break;                      /* cancelled */
            if (mode_ok(sr->sm_DisplayID)) {
                id = sr->sm_DisplayID;
                /* size and depth come from the mode itself: the requester's own width/height fields default to 640x480 */
                *w = (int)GetCyberIDAttr(CYBRIDATTR_WIDTH, id); *h = (int)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
                *depth = (int)GetCyberIDAttr(CYBRIDATTR_DEPTH, id);
                break;
            }
            {   struct EasyStruct es;                              /* the list also holds the Amiga chipset modes */
                es.es_StructSize = sizeof(struct EasyStruct); es.es_Flags = 0;
                es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)"That is not a usable RTG mode.\nPlease choose one of the listed RTG modes\n(640x480 up to 800x600).";
                es.es_GadgetFormat = (CONST_STRPTR)"Choose again";
                EasyRequestArgs(NULL, &es, NULL, NULL);
            }
        }
        FreeAslRequest(sr);
    }
    return id;
}

/* the best RTG mode for the wanted picture size (640x480 unless SIZE= says otherwise): 8 bit first (palette), otherwise a deeper one through a colour table */
static ULONG pick_mode(int *depth_out)
{
    static const int order[] = { 8, 16, 15, 24, 32 };
    for (int k = 0; k < 5; k++) {
        int d = order[k];
        ULONG id;
        if (force_depth && d != force_depth) continue;
        id = BestCModeIDTags(CYBRBIDTG_NominalWidth, req_w, CYBRBIDTG_NominalHeight, req_h, CYBRBIDTG_Depth, d, TAG_DONE);
        if (id == INVALID_ID || !IsCyberModeID(id)) continue;
        if (GetCyberIDAttr(CYBRIDATTR_WIDTH, id) < req_w || GetCyberIDAttr(CYBRIDATTR_HEIGHT, id) < req_h) continue;
        *depth_out = (int)GetCyberIDAttr(CYBRIDATTR_DEPTH, id);
        return id;
    }
    return INVALID_ID;
}

static int real_main(int argc, char **argv)
{
    /* argc == 0: started from Workbench, argv is not a string vector */
    const char *city;
    char city_buf[220] = "";
    int rc = 10, test_mode, depth = 8;
    struct Screen *scr = NULL;
    struct Window *win = NULL;
    struct Menu *menu = NULL;
    APTR vi = NULL;
    ULONG id;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) { printf("Micropolis needs AmigaOS 3.0 or newer.\n"); goto out; }
    read_tooltypes(argc, argv);
    prefs_load();
    debug_log = tt_debug;
    if (argc > 1 && strcasecmp(argv[1], "debug") && strncasecmp(argv[1], "depth=", 6) && strncasecmp(argv[1], "size=", 5) && strcasecmp(argv[1], "ask")) {
        city = argv[1];                                  /* given on the command line: relative to the current directory */
    } else {                                             /* default / CITY tooltype: relative to the program's drawer */
        const char *c = tt_city[0] ? tt_city : "cities/haight.cty";
        if (strchr(c, ':')) strncpy(city_buf, c, sizeof city_buf - 1);
        else sprintf(city_buf, "PROGDIR:%.180s", c);
        city = city_buf;
    }
    test_mode = argc > 2 && !strcmp(argv[2], "test");
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    { char b[80]; struct Task *t = FindTask(NULL);
      sprintf(b, "stack: %ld bytes", (long)((char *)t->tc_SPUpper - (char *)t->tc_SPLower)); logmsg(b); }
    logmsg("start");
    if (!CyberGfxBase && !test_mode) {
        fail("cybergraphics.library (version 40) not found.\nMicropolis needs an RTG graphics card with\nPicasso96 or CyberGraphX installed and running.");
        goto out;
    }
    logmsg("libs ok");
    if (!load_assets()) {
        fail("Cannot find the program data (res drawer).\nPlease keep the Micropolis program, its res and\ncities drawers together in one drawer.");
        goto out;
    }

    logmsg("assets ok");
    HomeDir = ""; ResourceDir = "PROGDIR:res"; HostName = "amiga";
    sim = MakeNewSim();
    logmsg("sim made");
    sim_init();
    logmsg("sim_init done");
    if (!LoadCity((char *)city)) { fail("Cannot load the city file '%s'.", city); goto out; }
    logmsg("city loaded");
    sprintf(status_msg, "Micropolis %s (%s)", VERSION, BUILD_REV);
    view_x = WORLD_X / 2 - VIEW_COLS / 2;
    view_y = WORLD_Y / 2 - VIEW_ROWS / 2;
    clamp_view();

    logmsg("view set");
    if (test_mode) { logmsg("test mode: stop before opening the screen"); rc = 0; goto out; }
    check_filter();
    if (size_forced) {                                    /* SIZE= : exactly that, no questions */
        id = pick_mode(&depth);
    } else {
        int use_saved = !ask_mode_flag && !pref_ask && pref_mode != 0xffffffffUL && mode_ok(pref_mode);
        if (use_saved) {                                  /* "always use this mode" */
            id = pref_mode;                                   /* size and depth from the mode (older prefs files saved the wrong size) */
            req_w = (int)GetCyberIDAttr(CYBRIDATTR_WIDTH, id); req_h = (int)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
            depth = (int)GetCyberIDAttr(CYBRIDATTR_DEPTH, id);
        } else {
            int w = 0, h = 0, d = 8;
            id = ask_mode(&w, &h, &d);
            if (id == INVALID_ID) {                       /* cancelled or no requester: take the default mode */
                logmsg("mode requester cancelled: default mode");
                id = pick_mode(&depth);
            } else {
                struct EasyStruct es;
                ULONG args[3];
                req_w = w; req_h = h; depth = d;
                es.es_StructSize = sizeof(struct EasyStruct); es.es_Flags = 0;
                es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)"Always use this screen mode (%ldx%ld, %ld bit)?\nOptions > \"Ask for screen mode at start\" turns the question on again.";
                es.es_GadgetFormat = (CONST_STRPTR)"Always use it|Ask me next time";
                args[0] = (ULONG)w; args[1] = (ULONG)h; args[2] = (ULONG)d;
                pref_ask = EasyRequestArgs(NULL, &es, NULL, (APTR)args) ? 0 : 1;
                pref_mode = id; pref_w = w; pref_h = h; pref_depth = d;
                prefs_save();
            }
        }
    }
    if (id == INVALID_ID) {
        fail("No suitable RTG screen mode found.\nMicropolis needs a mode of at least 640x480 pixels\nwith 8, 15, 16, 24 or 32 bit colour.");
        goto out;
    }
    scr_w = req_w < 640 ? 640 : req_w > MAX_W ? MAX_W : req_w;           /* requested screen size */
    scr_h = req_h < 480 ? 480 : req_h > MAX_H + 16 ? MAX_H + 16 : req_h;
    true_color = depth > 8;
    scr_mode_shown = (int)id; scr_depth_shown = depth;
    { char b[100]; sprintf(b, "screen mode 0x%08lx, depth %d", (unsigned long)id, depth); logmsg(b); }
    {   /* palette screens get Workbench-like window pens from our palette (grey borders, blue title bars) */
        static UWORD pens[] = { 0, PEN_GREY, 0, PEN_TEXT, 0, PEN_BTN, PEN_TEXT, PEN_GREY, PEN_TEXT, 0, PEN_GREY, 0, (UWORD)~0 };
        ULONG pen_tag = true_color ? TAG_IGNORE : SA_Pens;
        scr = OpenScreenTags(NULL, SA_DisplayID, id, SA_Width, scr_w, SA_Height, scr_h, SA_Depth, depth,
                             SA_Title, (ULONG)"Micropolis", SA_ShowTitle, TRUE, SA_Draggable, TRUE,
                             pen_tag, (ULONG)pens, TAG_DONE);
        if (!scr) {                                               /* some drivers insist on the mode's own size */
            logmsg("OpenScreen with our size failed: trying the mode's own size");
            scr = OpenScreenTags(NULL, SA_DisplayID, id, SA_Depth, depth, SA_Title, (ULONG)"Micropolis",
                                 SA_ShowTitle, TRUE, SA_Draggable, TRUE, pen_tag, (ULONG)pens, TAG_DONE);
        }
    }
    if (!scr) { fail("Cannot open the RTG screen (mode 0x%08lx, %d bit).", (unsigned long)id, depth); goto out; }
    g_scr = scr;
    logmsg("screen open");
    {   /* the game picture fills the screen below the title bar */
        int top = scr->BarHeight + 1;
        scr_w = scr->Width > MAX_W ? MAX_W : scr->Width;
        scr_h = scr->Height - top > MAX_H ? MAX_H : scr->Height - top;
        view_cols = (scr_w - TB_W) / 16; view_rows = (scr_h - BAR_H) / 16;
        fb = AllocVec(scr_w * scr_h, MEMF_ANY | MEMF_CLEAR);
        scratch = AllocVec(SCRATCH_STRIDE * MAX_H, MEMF_ANY | MEMF_CLEAR);
        if (!fb || !scratch) { fail("Not enough memory for a %dx%d screen.", scr_w, scr_h); goto out; }
        recenter(); clamp_view();
        if (true_color) {
            for (int i = 0; i < 256; i++)
                ctable[i] = ((ULONG)palette[i * 3] << 16) | ((ULONG)palette[i * 3 + 1] << 8) | palette[i * 3 + 2];
        } else {
            ULONG pal[2 + 256 * 3];
            pal[0] = (256UL << 16);
            for (int i = 0; i < 256 * 3; i++) pal[1 + i] = (ULONG)palette[i] * 0x01010101UL;
            pal[1 + 256 * 3] = 0;
            LoadRGB32(&scr->ViewPort, pal);
        }
        idcmp_port = CreateMsgPort();
        if (!idcmp_port) { fail("Cannot create a message port."); goto out; }
        win = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 0, WA_Top, top, WA_Width, scr_w,
                             WA_Height, scr_h, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
                             WA_ReportMouse, TRUE, WA_SimpleRefresh, TRUE, WA_IDCMP, 0, TAG_DONE);
        if (!win) { fail("Cannot open the game window."); goto out; }
        win->UserPort = idcmp_port;
        ModifyIDCMP(win, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_REFRESHWINDOW);
        main_window_ptr = win;
    }

    logmsg("window open");
    sound_init();
    if (GadToolsBase) {
        menu = make_menus(scr, &vi);
        g_menu = menu;
        if (menu) SetMenuStrip(win, menu);
    }
    editor.tool_state = queryState;
    setSpeed(3);
    if (argc > 3 && !strncmp(argv[3], "scroll=", 7)) { dbg_scroll = atoi(argv[3] + 7); setSpeed(0); }   /* debug: scroll test */
    if (argc > 3 && !strncmp(argv[3], "jump=", 5)) { int jx = 0, jy = 0; sscanf(argv[3] + 5, "%d,%d", &jx, &jy); view_x = jx; view_y = jy; clamp_view(); setSpeed(0); }
    if (argc > 3 && !strncmp(argv[3], "sound=", 6)) sound_play(argv[3] + 6);               /* debug: play a sound at start */
    if (argc > 3 && !strncmp(argv[3], "scen=", 5)) { int n = atoi(argv[3] + 5); LoadScenario((short)n); recenter(); notice_show(n); }   /* debug: start a scenario */
    if (argc > 3 && !strncmp(argv[3], "query=", 6)) { int qx = 60, qy = 50; sscanf(argv[3] + 6, "%d,%d", &qx, &qy); doZoneStatus((short)qx, (short)qy); }   /* debug */
    if (argc > 3 && !strcmp(argv[3], "modes")) {               /* debug: which display modes pass the filter */
        ULONG m = INVALID_ID; int n = 0;
        while ((m = NextDisplayInfo(m)) != INVALID_ID) {
            if (mode_ok(m)) { char lb[90]; sprintf(lb, "mode ok: 0x%08lx %ldx%ld %ld bit", (unsigned long)m, (long)GetCyberIDAttr(CYBRIDATTR_WIDTH, m), (long)GetCyberIDAttr(CYBRIDATTR_HEIGHT, m), (long)GetCyberIDAttr(CYBRIDATTR_DEPTH, m)); logmsg(lb); n++; }
        }
        { char lb[40]; sprintf(lb, "modes accepted: %d", n); logmsg(lb); }
    }
    if (argc > 3 && !strcmp(argv[3], "quake")) { MakeEarthquake(); }   /* debug */
    for (int ai = 1; ai < argc; ai++) if (!strncmp(argv[ai], "quit=", 5)) dbg_quit_loops = atoi(argv[ai] + 5) * 50;
    if (argc > 3 && !strncmp(argv[3], "show=", 5)) debug_show = 1;
    if (argc > 3 && !strcmp(argv[3], "show=about")) show_about();
    if (argc > 3 && !strcmp(argv[3], "show=eval")) open_info_panel(P_EVAL);              /* debug: open a panel at start */
    if (argc > 3 && !strcmp(argv[3], "show=graph")) open_info_panel(P_GRAPH);
    if (argc > 3 && !strcmp(argv[3], "show=map")) { open_info_panel(P_MAP); map_dirty = 1; }
    if (argc > 3 && !strncmp(argv[3], "notice=", 7)) { notice_show(atoi(argv[3] + 7)); notice_dirty = 1; }   /* debug */
    int running = 1, tick = 0, frame = 0, title_tick = 0;
    sound_ui_ready = 1;
    static const int delay[4] = { 0, 12, 4, 1 };      /* video frames per simulation step */
    while (running) {
        struct IntuiMessage *m;
        while ((m = (struct IntuiMessage *)GetMsg(idcmp_port))) {
            ULONG cls = m->Class;
            UWORD code = m->Code;
            int mx = m->MouseX, my = m->MouseY;
            struct Window *mw = m->IDCMPWindow;
            int pi = panel_index(mw);                             /* which panel window, or -1 = the game window */
            ReplyMsg((struct Message *)m);
            if (cls == IDCMP_MENUPICK) {
                while (code != MENUNULL && menu) {
                    struct MenuItem *it = ItemAddress(menu, code);
                    if (!it) break;
                    if (!do_command((int)(long)GTMENUITEM_USERDATA(it), win, it)) running = 0;
                    code = it->NextSelect;
                }
                dirty = 1;
                continue;
            }
            if (cls == IDCMP_CLOSEWINDOW) {                      /* close gadget of a panel window */
                if (pi >= 0) panels[pi].close();
                dirty = 1;
                continue;
            }
            if (cls == IDCMP_REFRESHWINDOW) {                    /* part of a window was uncovered: draw it again */
                BeginRefresh(mw);
                if (pi >= 0) panel_render(pi);
                else { full_redraw = 1; first_present = 1; toolbar_dirty = status_dirty = demand_dirty = 1; last_line1[0] = last_line2[0] = 0; render_frame(win); }
                EndRefresh(mw, TRUE);
                continue;
            }
            if (cls == IDCMP_RAWKEY) {
                if (code & IECODE_UP_PREFIX) continue;
                { static int n; if (n < 25) { char b[40]; sprintf(b, "rawkey 0x%02x", (unsigned)code); logmsg(b); n++; } }
                switch (code) {
                case 0x7a: view_y -= 2; break;                            /* mouse wheel */
                case 0x7b: view_y += 2; break;
                case 0x11: view_y--; break;                               /* W A S D scroll, too */
                case 0x21: view_y++; break;
                case 0x22: view_x++; break;
                case 0x20: view_x--; break;
                case 0x45: if (pi >= 0) panels[pi].close(); else running = 0; break;   /* Esc: closes the active panel window, else quits */
                case 0x44: case 0x40: if (pi >= 0) panels[pi].close(); break;          /* Return, Space */
                case 0x12: open_info_panel(P_EVAL); break;                    /* E */
                case 0x24: open_info_panel(P_GRAPH); break;                   /* G */
                case 0x4c: view_y--; break;                               /* cursor keys scroll */
                case 0x4d: view_y++; break;
                case 0x4e: view_x++; break;
                case 0x4f: view_x--; break;
                case 0x0a: setSpeed(0); break;                           /* 0 = pause */
                case 0x01: setSpeed(1); break;
                case 0x02: setSpeed(2); break;
                case 0x03: setSpeed(3); break;
                case 0x19: setSpeed(SimMetaSpeed ? 0 : 3); break;        /* P */
                case 0x35: DoBudgetFromMenu(); break;                      /* B */
                case 0x37: open_info_panel(P_MAP); map_dirty = 1; break;   /* M */
                case 0x36: if (map_visible) { map_mode = (map_mode + 1) % MAP_MODES; map_dirty = 1; sprintf(status_msg, "Map: %s", map_names[map_mode]); } break;   /* C: next overlay */
                }
                clamp_view();
            } else if (pi >= 0) {
                /* mouse in a panel window: window coordinates -> the panel's own coordinates */
                struct panel *pn = &panels[pi];
                int ax = mx - mw->BorderLeft + pn->ox, ay = my - mw->BorderTop + pn->oy;
                mouse_x = -1; hover_x = -1;                        /* the mouse left the game window: no edge scrolling, no cursor */
                if (pi == P_MAP) {                                 /* click or drag in the overview map: scroll there */
                    int tx, ty;
                    if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP) map_drag = 0;
                    else if (((cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) || (cls == IDCMP_MOUSEMOVE && map_drag)) &&
                             map_hit(ax, ay, 1, 1, &tx, &ty)) {
                        view_x = tx - VIEW_COLS / 2; view_y = ty - VIEW_ROWS / 2; clamp_view();
                        map_drag = 1;
                    }
                } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN && pn->click) {
                    pn->click(ax, ay);
                    *pn->dirty = 1;
                }
            } else {
                /* mouse in the game window: window coordinates -> tile / world pixel */
                int inview = mx >= TB_W && my >= BAR_H;
                int wx = (view_x << 4) + (mx - TB_W), wy = (view_y << 4) + (my - BAR_H);
                mouse_x = mx; mouse_y = my;
                if (inview) { hover_x = wx >> 4; hover_y = wy >> 4; } else hover_x = -1;
                if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                    if (mx < TB_W && my >= BAR_H) {
                        int i = (my - BAR_H) / TB_BTN_H;
                        if (i < 16) {
                            static const char *const tool_sound[18] = { "res", "com", "ind", "fire", "query", "police", "wire", "bulldozer", "rail",
                                                                         "road", "chalk", "eraser", "stadium", "park", "seaport", "coal", "nuclear", "airport" };
                            editor.tool_state = toolbtn[i].state; toolbar_dirty = 1;
                            if (UserSoundOn && editor.tool_state >= 0 && editor.tool_state < 18) sound_play(tool_sound[editor.tool_state]);   /* like the original palette */
                        }
                    } else if (inview) {
                        ToolDown(&editor, wx, wy);
                        dragging = 1;
                    }
                } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP) {
                    if (dragging && inview) ToolUp(&editor, wx, wy);
                    dragging = 0;
                } else if (cls == IDCMP_MOUSEMOVE && dragging && inview) {
                    ToolDrag(&editor, wx, wy);
                }
            }
            dirty = 1;
        }
        if (mouse_x >= 0 && !panning && (++frame & 3) == 0) {   /* edge scrolling */
            int ox = view_x, oy = view_y;
            if (mouse_x >= TB_W && mouse_x < TB_W + 6) view_x--;
            if (mouse_x >= SCR_W - 6) view_x++;
            if (mouse_y >= BAR_H && mouse_y < BAR_H + 4) view_y--;
            if (mouse_y >= SCR_H - 4) view_y++;
            clamp_view();
            if (ox != view_x || oy != view_y) dirty = 1;
        }
        sound_poll();
        if (ShakeNow > 0 && shake_frames == 0) { shake_frames = 50; shake_bx = view_x; shake_by = view_y; }
        if (shake_frames > 0) {
            shake_frames--;
            if (shake_frames == 0) { view_x = shake_bx; view_y = shake_by; ShakeNow = 0; }
            else { view_x = shake_bx + (rand() % 3) - 1; view_y = shake_by + (rand() % 3) - 1; }
            clamp_view(); dirty = 1;
        }
        if (debug_show && notice_visible && !(argc > 3 && !strcmp(argv[3], "show=about"))) notice_close();
        if (dbg_scroll > 0 && (frame & 3) == 0) { view_x++; if (dbg_scroll & 1) view_y++; dbg_scroll--; clamp_view(); dirty = 1; }
        if (SimSpeed && ++tick >= delay[SimSpeed]) {
            struct timeval t0, t1;
            tick = 0;
            gettimeofday(&t0, NULL);
            sim_loop(1);
            gettimeofday(&t1, NULL);
            stats.sim_us += (long)(t1.tv_sec - t0.tv_sec) * 1000000L + (long)(t1.tv_usec - t0.tv_usec);
            stats.sim_steps++;
            dirty = 1;
        }
        if (dirty) {
            static int first = 1; if (first) { logmsg("first frame"); first = 0; }
            if (!TilesAnimated) { animateTiles(); TilesAnimated = 1; }
            render_frame(win);
            dirty = 0;
        }
        if ((++title_tick & 7) == 0) {                                    /* the screen title bar shows the city's numbers */
            char t[120];
            sprintf(t, "Micropolis %s   %s   %s   Pop %ld   %s", VERSION, status_date, status_funds, (long)CityPop,
                    SimSpeed ? "" : "(paused)");
            set_screen_title(t);
        }
        sync_panels();
        if (dbg_quit_loops && title_tick > dbg_quit_loops) running = 0;
        WaitTOF();
        gettimeofday(&now_time, NULL);
        flagBlink = (now_time.tv_usec < 500000) ? 1 : -1;
    }
    rc = 0;
out:
    sound_exit();
    logmsg("cleanup");
    for (int i = 0; i < NPANELS; i++) panel_close(i);
    if (win) close_window_safely(win);
    main_window_ptr = NULL;
    if (menu) FreeMenus(menu);
    if (vi) FreeVisualInfo(vi);
    if (idcmp_port) DeleteMsgPort(idcmp_port);
    logmsg("window closed");
    if (scr) CloseScreen(scr);
    logmsg("screen closed");
    if (fb) FreeVec(fb);
    if (scratch) FreeVec(scratch);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (AslBase) CloseLibrary(AslBase);
    if (IconBase) CloseLibrary(IconBase);
    logmsg("cleanup done");
    return rc;
}

static void at_exit_log(void) { app_log("atexit handler"); }
static void __attribute__((destructor)) dtor_log(void) { app_log("destructor"); }

int main(int argc, char **argv)
{
    atexit(at_exit_log);
    int rc = run_with_stack(real_main, argc, argv, BIG_STACK);
    app_log("main() is returning");
    return rc;
}
