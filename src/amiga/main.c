/* Micropolis for AmigaOS: Intuition/cybergraphics (RTG) frontend for the simulation core.
 * Milestone 3: map view with scrolling, animated tiles, sprites, status line, speed control. */
/* sim.h declares a GetSprite() that clashes with graphics.library's */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#include "stack.h"
#undef GetSprite
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

struct Library *CyberGfxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#define SCR_W 640
#define SCR_H 480
#define BAR_H 32                       /* status bar on top */
#define VIEW_COLS (SCR_W / 16)         /* 40 */
#define VIEW_ROWS ((SCR_H - BAR_H) / 16) /* 28 */

#define PEN_BAR   32                   /* extra palette entries behind the game's own colors */
#define PEN_TEXT  33

static void logmsg(const char *msg)   /* progress log: when started from the Workbench there is no console */
{
    FILE *f = fopen("micropolis.log", "a");
    if (f) { fputs(msg, f); fputc(10, f); fclose(f); }
}

static UBYTE *tiles;                   /* 960 * 256 palette indices */
static UBYTE *objects;                 /* objects.bin */
static UBYTE *fb;                      /* SCR_W x SCR_H chunky frame buffer */
static UBYTE palette[256 * 3];

static int view_x, view_y;             /* top-left tile of the view */
static char status_date[32] = "", status_funds[48] = "", status_msg[96] = "";
static int dirty = 1;

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
    UBYTE *pal = load_file("res/palette.bin", NULL);
    tiles = load_file("res/tiles.bin", NULL);
    objects = load_file("res/objects.bin", NULL);
    if (!pal || !tiles || !objects) { printf("Assets fehlen (res/*.bin)\n"); return 0; }
    memcpy(palette, pal, 256 * 3);
    FreeVec(pal);
    palette[PEN_BAR * 3 + 0] = 0x20; palette[PEN_BAR * 3 + 1] = 0x28; palette[PEN_BAR * 3 + 2] = 0x50;
    palette[PEN_TEXT * 3 + 0] = 0xff; palette[PEN_TEXT * 3 + 1] = 0xff; palette[PEN_TEXT * 3 + 2] = 0xff;
    return 1;
}

/* ---------- drawing ---------- */
static void draw_tile(int col, int row, int tile)
{
    UBYTE *src = tiles + tile * 256;
    UBYTE *dst = fb + (BAR_H + row * 16) * SCR_W + col * 16;
    for (int y = 0; y < 16; y++, src += 16, dst += SCR_W)
        memcpy(dst, src, 16);
}

static void draw_sprite(SimSprite *s)
{
    if (s->frame == 0) return;
    UWORD n = *(UWORD *)objects;
    UBYTE *tab = objects + 2;
    for (UWORD i = 0; i < n; i++, tab += 8) {
        if (tab[0] != s->type || tab[1] != s->frame - 1) continue;
        int w = tab[2], h = tab[3];
        UBYTE *px = objects + *(ULONG *)(tab + 4);
        int x0 = s->x - (view_x << 4) + s->x_offset;
        int y0 = s->y - (view_y << 4) + s->y_offset + BAR_H;
        for (int y = 0; y < h; y++) {
            int dy = y0 + y;
            if (dy < BAR_H || dy >= SCR_H) continue;
            for (int x = 0; x < w; x++) {
                int dx = x0 + x;
                UBYTE c = px[y * w + x];
                if (c && dx >= 0 && dx < SCR_W) fb[dy * SCR_W + dx] = c;
            }
        }
        return;
    }
}

static void render_map(void)
{
    int blink = flagBlink > 0;
    for (int row = 0; row < VIEW_ROWS; row++) {
        for (int col = 0; col < VIEW_COLS; col++) {
            int mx = view_x + col, my = view_y + row;
            int tile = 0;
            if (mx < WORLD_X && my < WORLD_Y) {
                tile = (unsigned short)Map[mx][my];
                if ((tile & LOMASK) >= TILE_COUNT) tile -= TILE_COUNT;
                if (blink && (tile & ZONEBIT) && !(tile & PWRBIT)) tile = LIGHTNINGBOLT;
                else tile &= LOMASK;
            }
            draw_tile(col, row, tile);
        }
    }
    for (SimSprite *s = sim->sprite; s; s = s->next) draw_sprite(s);
}

static void present(struct Window *win)
{
    struct RastPort *rp = win->RPort;
    WritePixelArray(fb, 0, 0, SCR_W, rp, 0, 0, SCR_W, SCR_H, RECTFMT_LUT8);
    char line[200];
    SetAPen(rp, PEN_TEXT);
    SetBPen(rp, PEN_BAR);
    sprintf(line, " %-9s  %-22s  Pop %-8ld  Speed %d%s ", status_date, status_funds, (long)CityPop, (int)SimSpeed,
            SimSpeed ? "" : " (Pause)");
    Move(rp, 4, 12);  Text(rp, (CONST_STRPTR)line, strlen(line));
    sprintf(line, " %-70s", status_msg);
    Move(rp, 4, 27);  Text(rp, (CONST_STRPTR)line, strlen(line));
}

static void clamp_view(void)
{
    if (view_x > WORLD_X - VIEW_COLS) view_x = WORLD_X - VIEW_COLS;
    if (view_y > WORLD_Y - VIEW_ROWS) view_y = WORLD_Y - VIEW_ROWS;
    if (view_x < 0) view_x = 0;
    if (view_y < 0) view_y = 0;
}

static int real_main(int argc, char **argv)
{
    /* argc == 0: started from Workbench, argv is not a string vector */
    const char *city = argc > 1 ? argv[1] : "cities/haight.cty";
    int rc = 10;
    struct Screen *scr = NULL;
    struct Window *win = NULL;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    { char b[80]; struct Task *t = FindTask(NULL);
      sprintf(b, "stack: %ld bytes", (long)((char *)t->tc_SPUpper - (char *)t->tc_SPLower)); logmsg(b); }
    logmsg("start");
    logmsg("libs ok");
    if (!load_assets()) goto out;
    fb = AllocVec(SCR_W * SCR_H, MEMF_ANY | MEMF_CLEAR);
    if (!fb) goto out;

    logmsg("assets ok");
    HomeDir = ""; ResourceDir = "res"; HostName = "amiga";
    sim = MakeNewSim();
    logmsg("sim made");
    sim_init();
    logmsg("sim_init done");
    if (!LoadCity((char *)city)) { printf("Stadt nicht ladbar: %s\n", city); logmsg("LoadCity failed"); logmsg(city); goto out; }
    logmsg("city loaded");
    view_x = WORLD_X / 2 - VIEW_COLS / 2;
    view_y = WORLD_Y / 2 - VIEW_ROWS / 2;
    clamp_view();

    logmsg("view set");
    if (argc > 2 && !strcmp(argv[2], "test")) { logmsg("test mode: stop before opening the screen"); rc = 0; goto out; }
    if (!CyberGfxBase) { printf("cybergraphics.library fehlt (Picasso96 nicht aktiv)\n"); logmsg("no cybergraphics.library"); goto out; }
    ULONG id = BestCModeIDTags(CYBRBIDTG_NominalWidth, SCR_W, CYBRBIDTG_NominalHeight, SCR_H,
                               CYBRBIDTG_Depth, 8, TAG_DONE);
    if (id == INVALID_ID) { printf("kein RTG-Modus %dx%d gefunden\n", SCR_W, SCR_H); goto out; }
    scr = OpenScreenTags(NULL, SA_DisplayID, id, SA_Width, SCR_W, SA_Height, SCR_H, SA_Depth, 8,
                         SA_Quiet, TRUE, SA_ShowTitle, FALSE, TAG_DONE);
    if (!scr) { printf("OpenScreen fehlgeschlagen\n"); goto out; }
    logmsg("screen open");
    ULONG pal[2 + 256 * 3];
    pal[0] = (256UL << 16);
    for (int i = 0; i < 256 * 3; i++) pal[1 + i] = (ULONG)palette[i] * 0x01010101UL;
    pal[1 + 256 * 3] = 0;
    LoadRGB32(&scr->ViewPort, pal);
    win = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 0, WA_Top, 0, WA_Width, SCR_W,
                         WA_Height, SCR_H, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
                         WA_RMBTrap, TRUE, WA_IDCMP, IDCMP_RAWKEY, TAG_DONE);
    if (!win) { printf("OpenWindow fehlgeschlagen\n"); goto out; }

    logmsg("window open");
    SimSpeed = 3;
    int running = 1, tick = 0;
    static const int delay[4] = { 0, 12, 4, 1 };      /* video frames per simulation step */
    while (running) {
        struct IntuiMessage *m;
        while ((m = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            UWORD code = m->Code;
            ReplyMsg((struct Message *)m);
            if (code & IECODE_UP_PREFIX) continue;
            switch (code) {
            case 0x45: running = 0; break;                            /* Esc */
            case 0x4c: view_y--; break;                               /* cursor keys scroll */
            case 0x4d: view_y++; break;
            case 0x4e: view_x++; break;
            case 0x4f: view_x--; break;
            case 0x0a: SimSpeed = 0; break;                           /* 0 = pause */
            case 0x01: SimSpeed = 1; break;
            case 0x02: SimSpeed = 2; break;
            case 0x03: SimSpeed = 3; break;
            case 0x19: SimSpeed = SimSpeed ? 0 : 3; break;            /* P */
            }
            clamp_view();
            dirty = 1;
        }
        if (SimSpeed && ++tick >= delay[SimSpeed]) {
            tick = 0;
            sim_loop(1);
            dirty = 1;
        }
        if (dirty) {
            static int first = 1; if (first) { logmsg("first frame"); first = 0; }
            if (!TilesAnimated) { animateTiles(); TilesAnimated = 1; }
            render_map();
            present(win);
            dirty = 0;
        }
        WaitTOF();
        gettimeofday(&now_time, NULL);
        flagBlink = (now_time.tv_usec < 500000) ? 1 : -1;
    }
    rc = 0;
out:
    if (win) CloseWindow(win);
    if (scr) CloseScreen(scr);
    if (fb) FreeVec(fb);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    return rc;
}

int main(int argc, char **argv)
{
    return run_with_stack(real_main, argc, argv, BIG_STACK);
}
