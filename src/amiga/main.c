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
#include "sound_ui.h"
#include "coast.h"
#include "wind.h"
#include "l10n.h"
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
#include <proto/keymap.h>
#include <libraries/amigaguide.h>
#include <proto/amigaguide.h>
#include <devices/inputevent.h>
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
struct Library *KeymapBase;
struct Library *AmigaGuideBase;
static APTR guide_handle;
static char guide_path[200];                       /* open help document */

/* Screen size: 640x480 by default, up to MAX_W x MAX_H (ToolType SIZE=800x600, or MODE=ASK for a mode requester).
 * The tile view fills the screen, so a bigger screen shows more of the city. */
#define MAX_W 800                       /* largest picture: 800x600 (the useful range on RTG cards and emulators) */
#define MAX_H 600
static int scr_w = 640, scr_h = 480;
#define SCR_W scr_w
#define SCR_H scr_h
#define BAR_H 32                       /* status bar on top */
#define TB_W 144                       /* tool palette on the left */
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
static int pref_level;                          /* difficulty 0 easy, 1 medium, 2 hard */
enum { O_BULLDOZE, O_BUDGET, O_DISASTERS, O_SOUND, O_AUTOGO, O_ANIM, O_MSGS, O_NOTICES, O_SPEAK, O_STARTMENU, O_CLASSIC, O_AMBIENT, O_WORLD, N_OPT };
static int classic_icons;                        /* 1 = the original tool palette pictures, 0 = the uniform 32x32 set */
static int start_pending;
static int refresh_full;                   /* the next window refresh must be followed by a complete redraw (the picture changed while a requester covered it) */
static int start_menu = 1;                       /* ask what to start with: new city, load, scenario */
static const char *const opt_key[N_OPT] = { "AUTOBULLDOZE", "AUTOBUDGET", "DISASTERS", "SOUND", "AUTOGOTO", "ANIMATION", "MESSAGES", "MESSAGEWINDOWS", "SPEAKTOOLS", "STARTMENU", "CLASSICICONS", "AMBIENT", "WORLDANIM" };
static int world_anim = 1;                       /* surf along the coasts and gusts of wind over woods (the 'city' animation of cars and smoke is the core's DoAnimation) */
static int pref_opt[N_OPT] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };     /* -1 = not saved yet: keep the default */
static int pref_speed = -1;
static int pref_fxvol = -1, pref_ambvol = -1;      /* volume sliders of the sound window (-1 = default) */
static char pref_lang[24];                       /* "" = the system's preferred language, else "english" "deutsch" "polski" */
static char arg_lang[24];                        /* lang= argument or LANGUAGE tooltype: wins over the preferences */
#define N_WINPOS 7                              /* NPANELS */
static int pref_wx[N_WINPOS] = { -1, -1, -1, -1, -1, -1, -1 }, pref_wy[N_WINPOS] = { -1, -1, -1, -1, -1, -1, -1 };   /* remembered window positions */
static int opt_get(int o)
{
    switch (o) {
    case O_BULLDOZE: return autoBulldoze;  case O_BUDGET: return autoBudget;  case O_DISASTERS: return !NoDisasters;
    case O_SOUND: return UserSoundOn;      case O_AUTOGO: return autoGo;      case O_ANIM: return DoAnimation;
    case O_MSGS: return DoMessages;        case O_SPEAK: return speak_tools;   case O_STARTMENU: return start_menu;   case O_CLASSIC: return classic_icons;   case O_AMBIENT: return ambient_on;   case O_WORLD: return world_anim;   default: return DoNotices;
    }
}
static void opt_set(int o, int v)
{
    switch (o) {
    case O_BULLDOZE: autoBulldoze = v; break;  case O_BUDGET: autoBudget = v; break;  case O_DISASTERS: NoDisasters = !v; break;
    case O_SOUND: UserSoundOn = v; break;      case O_AUTOGO: autoGo = v; break;      case O_ANIM: DoAnimation = v; break;
    case O_MSGS: DoMessages = v; break;        case O_SPEAK: speak_tools = v; break;   case O_STARTMENU: start_menu = v; break;   case O_CLASSIC: classic_icons = v; break;   case O_AMBIENT: ambient_on = v; break;   case O_WORLD: world_anim = v; break;   default: DoNotices = v; break;
    }
}

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
            else if (!strncmp(line, "LANGUAGE=", 9)) { strncpy(pref_lang, line + 9, sizeof pref_lang - 1); pref_lang[strcspn(pref_lang, "\r\n")] = 0; }
            else if (!strncmp(line, "SPEED=", 6)) { pref_speed = atoi(line + 6); if (pref_speed < 0 || pref_speed > 3) pref_speed = -1; }
            else if (!strncmp(line, "SFXVOL=", 7)) { pref_fxvol = atoi(line + 7); if (pref_fxvol < 0 || pref_fxvol > 100) pref_fxvol = -1; }
            else if (!strncmp(line, "AMBVOL=", 7)) { pref_ambvol = atoi(line + 7); if (pref_ambvol < 0 || pref_ambvol > 100) pref_ambvol = -1; }
            else if (!strncmp(line, "WIN", 3) && line[3] >= '0' && line[3] < '0' + N_WINPOS && line[4] == '=') {
                int x, y;
                if (sscanf(line + 5, "%d,%d", &x, &y) == 2) { pref_wx[line[3] - '0'] = x; pref_wy[line[3] - '0'] = y; }
            }
            else {
                int o;
                for (o = 0; o < N_OPT; o++) { size_t n = strlen(opt_key[o]); if (!strncmp(line, opt_key[o], n) && line[n] == '=') { pref_opt[o] = atoi(line + n + 1) ? 1 : 0; break; } }
                if (o < N_OPT) continue;
            }
            if (!strncmp(line, "LEVEL=", 6)) { pref_level = atoi(line + 6); if (pref_level < 0 || pref_level > 2) pref_level = 0; }
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
        fprintf(f, "ASK=%d\nMODE=%lx\nSIZE=%dx%d\nDEPTH=%d\nLEVEL=%d\n", pref_ask, (unsigned long)pref_mode, pref_w, pref_h, pref_depth, pref_level);
        for (int o = 0; o < N_OPT; o++) fprintf(f, "%s=%d\n", opt_key[o], pref_opt[o] >= 0 ? pref_opt[o] : opt_get(o));
        fprintf(f, "SPEED=%d\n", pref_speed >= 0 ? pref_speed : 3);
        fprintf(f, "SFXVOL=%d\nAMBVOL=%d\n", sound_fx_vol, sound_amb_vol);
        fprintf(f, "LANGUAGE=%s\n", pref_lang);
        for (int i = 0; i < N_WINPOS; i++) if (pref_wx[i] >= 0) fprintf(f, "WIN%d=%d,%d\n", i, pref_wx[i], pref_wy[i]);
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
static long clean_time;                    /* CityTime when the city was loaded, created or saved */
static int tool_used;                      /* a building tool was used since then */
static int menu_sync_pending;              /* set by UISetOptions / UISetGameLevel: the menu check marks must follow the game state */
static int save_state;                     /* 1 = the core saved the city, 2 = it could not (save_msg) */
static char save_msg[300];
static int dirty = 1;
static int toolbar_dirty_ext;              /* set by ui_update_heads(), picked up by the renderer */
static int map_dirty = 1;
static int sound_dirty = 1;
static int budget_dirty = 1, notice_dirty = 1, eval_dirty = 1, graph_dirty = 1, query_dirty = 1;
static int query_visible;
static char query_f[6][24];                /* zone, density, value, crime, pollution, growth of the queried tile */   /* a panel must be redrawn */
static int demand_r, demand_c, demand_i, demand_dirty = 1;   /* -15..15, from "UISetDemand" */
static void clamp_view(void);

static SimView editor;                 /* the core's tools take a SimView; only tool_state/last_x/y are used */
static int hover_x = -1, hover_y = -1; /* tile under the mouse, -1 = none */
static int dragging;
static int drag_x, drag_y;                 /* where the tool was last dragged (world pixels) */
static void end_drag(void)                 /* the button went up somewhere else (a window opened over the map): finish the line where it was, do not let it follow the mouse */
{
    if (dragging) { ToolUp(&editor, drag_x, drag_y); dragging = 0; }
}
static int map_drag;
static int sound_ui_ready;                /* set when the main loop runs: UI sounds (tool select, speed) only from then on */
static int shake_frames, shake_bx, shake_by;   /* earthquake: the view jitters by a tile for about a second */
static int dbg_about, dbg_det;                 /* test options: keep the About box open / deterministic picture (no blinking, no animation, no ambient sound) */
static char dbg_dump[120];                  /* test option dump=PATH: write the frame buffer and the panel scratch buffer at exit */
static int dbg_quit_loops;                 /* test option quit=N: leave the game after N seconds (checks the clean shutdown) */
static int dbg_scroll;                  /* test option scroll=N: scroll N tiles (x and y), then stop */
static int debug_show;                  /* test option show=eval|graph: keep messages away so the panel can be photographed */
static const int panning = 0;            /* (right button is the menu button now) */
static int mouse_x = -1, mouse_y = -1;   /* last window position, for edge scrolling */

#define NTOOLS 18
static UBYTE *toolicons2;              /* toolicons2.bin: uniform 32x32 set (colours from pen 48 up) */
static int toolicons2_n, toolicons2_t;
static UBYTE *toolicons;               /* toolicons.bin: the original's tool palette pictures (normal and highlighted) */
static const struct { const char *name; int cost; short x, y; } tooldef[NTOOLS] = {   /* indexed by tool state; positions as in the original palette */
    { N_("Residential"), 100, 13, 58 },  { N_("Commercial"), 100, 51, 58 },   { N_("Industrial"), 100, 89, 58 },
    { N_("Fire Station"), 500, 13, 112 }, { N_("Query"), 0, 51, 112 },        { N_("Police Station"), 500, 89, 112 },
    { N_("Wire"), 5, 32, 150 },          { N_("Bulldozer"), 1, 70, 150 },
    { N_("Rail"), 20, 10, 188 },         { N_("Road"), 10, 70, 188 },
    { N_("Chalk"), 0, 32, 216 },         { N_("Eraser"), 0, 70, 216 },
    { N_("Stadium"), 5000, 5, 254 },     { N_("Park"), 10, 51, 254 },         { N_("Seaport"), 3000, 89, 254 },
    { N_("Coal Power"), 3000, 5, 300 },  { N_("Nuclear Power"), 5000, 89, 300 }, { N_("Airport"), 10000, 39, 346 },
};

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
    if (!strncmp(cmd, "UISetDate ", 10))         {
        copy_braced(status_date, sizeof status_date, cmd); dirty = 1;
        if (strlen(status_date) > 3) {                                    /* "Jan 2003": the month names are translated */
            char mon[4], rest[32];
            memcpy(mon, status_date, 3); mon[3] = 0;
            strcpy(rest, status_date + 3);
            sprintf(status_date, "%.5s%s", T(mon), rest);
        }
    }
    else if (!strncmp(cmd, "UISetFunds ", 11))   {
        copy_braced(status_funds, sizeof status_funds, cmd); dirty = 1;
        if (!strncmp(status_funds, "Funds:", 6)) { char rest[48]; strcpy(rest, status_funds + 6); snprintf(status_funds, sizeof status_funds, "%s%s", T("Funds:"), rest); }   /* "Funds: $1,000" */
    }
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
    else if (!strncmp(cmd, "UISetOptions ", 13) || !strncmp(cmd, "UISetGameLevel ", 15)) { menu_sync_pending = 1; dirty = 1; }   /* a loaded city brings its own options and level: show them in the menu */
    else if (!strncmp(cmd, "UISetBudget ", 12))       { budget_set_totals(cmd); budget_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UISetBudgetValues ", 18)) { budget_set_values(cmd); dirty = 1; }
    else if (!strncmp(cmd, "UIShowBudgetAndWait", 19)) { budget_show(); budget_dirty = 1; dirty = 1; }
    else if (!strncmp(cmd, "UISetDemand ", 12)) {
        int r, c, i;
        if (sscanf(cmd + 12, "%d %d %d", &r, &c, &i) == 3) { demand_r = r; demand_c = c; demand_i = i; demand_dirty = 1; dirty = 1; }
    }
    else if (!strncmp(cmd, "UIShowPicture ", 14))   { if (!debug_show && DoNotices) { notice_show(atoi(cmd + 14)); notice_dirty = 1; } dirty = 1; }
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
    else if (!strncmp(cmd, "UIDidSaveCity", 13))   { save_state = 1; }
    else if (!strncmp(cmd, "UIDidntSaveCity ", 16)) { save_state = 2; copy_braced(save_msg, sizeof save_msg, cmd); }
    else if (!strncmp(cmd, "UISetSpeed ", 11))      { if (sound_ui_ready && UserSoundOn) sound_play("boing"); }
    else if (!strncmp(cmd, "UISetMessage ", 13)) { if (DoMessages) { char m[200]; m[0] = 0; copy_braced(m, sizeof m, cmd); strncpy(status_msg, T(m), sizeof status_msg - 1); status_msg[sizeof status_msg - 1] = 0; } dirty = 1; }
    return 0;
}
void ui_invalidate(int what) { dirty = 1; }
void ui_flush(void) {}
void ui_update_heads(void) { toolbar_dirty_ext = 1; dirty = 1; }   /* the core changed the tool (keyboard shortcut) */

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
    toolicons = load_file("PROGDIR:res/toolicons.bin", NULL);
    toolicons2 = load_file("PROGDIR:res/toolicons2.bin", NULL);
    if (toolicons2) { toolicons2_n = *(UWORD *)toolicons2; toolicons2_t = *(UWORD *)(toolicons2 + 2); }
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
    coast_init(tiles, pal);
    coast_palette(palette);
    wind_init(tiles); wind_palette(palette);
    FreeVec(pal);
    if (toolicons2) for (int i = 0; i < toolicons2_n && 48 + i < 256; i++) memcpy(&palette[(48 + i) * 3], toolicons2 + 4 + i * 3, 3);
    palette[PEN_BAR * 3 + 0] = 0x20; palette[PEN_BAR * 3 + 1] = 0x28; palette[PEN_BAR * 3 + 2] = 0x50;
    palette[PEN_TEXT * 3 + 0] = 0xff; palette[PEN_TEXT * 3 + 1] = 0xff; palette[PEN_TEXT * 3 + 2] = 0xff;
    palette[PEN_SEL * 3 + 0] = 0xb0; palette[PEN_SEL * 3 + 1] = 0x60; palette[PEN_SEL * 3 + 2] = 0x20;
    {   static const UBYTE v[7][3] = { {0xbf,0xbf,0xbf}, {0xff,0xff,0x00}, {0xff,0x7f,0x00}, {0xff,0x00,0x00},
                                       {0x00,0x7f,0x00}, {0x00,0xe6,0x00}, {0x66,0x66,0xe6} };
        for (int i = 0; i < 7; i++) memcpy(&palette[(PEN_V_LGRAY + i) * 3], v[i], 3);
    }
    palette[PEN_BTN * 3 + 0] = 0x40; palette[PEN_BTN * 3 + 1] = 0x48; palette[PEN_BTN * 3 + 2] = 0x70;
    palette[PEN_GREY * 3 + 0] = 0xaa; palette[PEN_GREY * 3 + 1] = 0xaa; palette[PEN_GREY * 3 + 2] = 0xaa;
    /* Menus highlight by complementing the pixels: pen p becomes 255-p. Menu text (pen 0) turns into pen 255 and the grey
       menu background into pen 255-PEN_GREY, so those two must be readable colours (they were black: unreadable). */
    if (!toolicons2 || 48 + toolicons2_n <= 255 - PEN_GREY) {
        palette[(255 - PEN_GREY) * 3 + 0] = 0xff; palette[(255 - PEN_GREY) * 3 + 1] = 0xff; palette[(255 - PEN_GREY) * 3 + 2] = 0xff;
        palette[255 * 3 + 0] = 0x00; palette[255 * 3 + 1] = 0x00; palette[255 * 3 + 2] = 0x88;
    }
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
static int tb_label_only;                  /* only the name/cost label at the top of the palette changed (mouse over an icon): send just that part */
static int last_view_x = -1, last_view_y = -1;
#define MAX_SPR 48
static struct rect prev_spr[MAX_SPR];
static int n_prev_spr;
static struct rect prev_cursor;
static int have_prev_cursor;
static char last_line1[200], last_line2[200];

static struct { long frames, tiles, bytes, render_us, sim_us, sim_steps; } stats;   /* no doubles: the ieee double library is unreliable */

static int anim_hold, anim_slow;                  /* the animation stands still while the view scrolls (anim_hold frames); anim_slow: a slow machine, half the speed */
static int coast_tick, coast_seen;                /* animation step of the surf (145 ms each); shore tiles seen in the last picture */
static int coast_freeze = -1;                     /* debug: show this step only */

static void draw_tile(int col, int row, int tile)
{
    const UBYTE *src = tiles + tile * 256;
    UBYTE *dst = fb + (BAR_H + row * 16) * SCR_W + TB_W + col * 16;
    for (int y = 0; y < 16; y++, src += 16, dst += SCR_W)
        memcpy(dst, src, 16);
    if (world_anim && coast_has(tile))                           /* surf along the shore */
        coast_draw(fb + (BAR_H + row * 16) * SCR_W + TB_W + col * 16, SCR_W, tile, (view_x + col) << 4, (view_y + row) << 4, coast_tick);
    if (world_anim && wind_has(tile))                            /* gusts over the woods */
        wind_draw(fb + (BAR_H + row * 16) * SCR_W + TB_W + col * 16, SCR_W, tile, (view_x + col) << 4, (view_y + row) << 4, coast_tick);
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

static int tool_x(int i)  { return (classic_icons || !toolicons2) ? tooldef[i].x : 8 + (i % 3) * 40; }
static int tool_y(int i)  { return (classic_icons || !toolicons2) ? tooldef[i].y : 58 + (i / 3) * 38; }
static int tool_w(int i)  { return (classic_icons || !toolicons2) ? (toolicons ? toolicons[2 + i * 10] : 34) : toolicons2_t; }
static int tool_h(int i)  { return (classic_icons || !toolicons2) ? (toolicons ? toolicons[2 + i * 10 + 1] : 34) : toolicons2_t; }

static int tool_hover = -1;            /* tool icon under the mouse: its name and cost show in the label (a fixed-place tooltip) */

static int tool_at(int mx, int my)
{
    if (mx >= TB_W || my < BAR_H) return -1;
    for (int i = 0; i < NTOOLS; i++) {
        if (mx >= tool_x(i) && mx < tool_x(i) + tool_w(i) && my >= BAR_H + tool_y(i) && my < BAR_H + tool_y(i) + tool_h(i)) return i;
    }
    return -1;
}

static void draw_toolbar(void)
{
    char line[40];
    for (int y = BAR_H; y < SCR_H; y++) memset(fb + y * SCR_W, PEN_BAR, TB_W);
    if (!classic_icons && toolicons2) {
        const UBYTE *px0 = toolicons2 + 4 + toolicons2_n * 3;
        int t = toolicons2_t;
        for (int i = 0; i < NTOOLS; i++) {
            const UBYTE *px = px0 + (long)(i * 2 + (i == editor.tool_state ? 1 : 0)) * t * t;
            for (int y = 0; y < t; y++) {
                int dy = BAR_H + tool_y(i) + y;
                if (dy >= SCR_H) break;
                memcpy(fb + dy * SCR_W + tool_x(i), px + y * t, t);
            }
        }
    } else if (toolicons) {
        const UBYTE *tab = toolicons + 2;
        for (int i = 0; i < NTOOLS; i++) {
            int w = tab[i * 10], h = tab[i * 10 + 1];
            const UBYTE *px = toolicons + *(const ULONG *)(tab + i * 10 + (i == editor.tool_state ? 6 : 2));
            for (int y = 0; y < h; y++) {
                int dy = BAR_H + tooldef[i].y + y;
                if (dy >= SCR_H) break;
                for (int x = 0; x < w; x++) fb[dy * SCR_W + tooldef[i].x + x] = px[y * w + x] ? px[y * w + x] : PEN_BAR;
            }
        }
    }
    {   int t = tool_hover >= 0 ? tool_hover : editor.tool_state;              /* name and cost of the tool under the mouse, else of the chosen one */
        if (t >= 0 && t < NTOOLS) {
            gfx_target(fb, SCR_W);
            gfx_colors(tool_hover >= 0 ? PEN_V_YELLOW : PEN_TEXT, PEN_BAR);
            sprintf(line, "%-17.17s", T(tooldef[t].name));
            gfx_print(6, BAR_H + 22, line);
            if (tooldef[t].cost) sprintf(line, T("Cost: $%d"), tooldef[t].cost); else strcpy(line, "                ");
            gfx_print(6, BAR_H + 40, line);
        }
    }
}

/* chalk notes: world pixel polylines. Cells they touch (now or in the last frame) are redrawn every frame, then the lines go on top. */
static UBYTE ink_cur[WORLD_Y][WORLD_X], ink_prev[WORLD_Y][WORLD_X];
static int ink_prev_any;

static void ink_mark(int wx, int wy)
{
    int cx = wx >> 4, cy = wy >> 4;
    if (cx >= 0 && cx < WORLD_X && cy >= 0 && cy < WORLD_Y) ink_cur[cy][cx] = 1;
}

static void ink_pixel(int wx, int wy, UBYTE pen)
{
    int x = TB_W + wx - (view_x << 4), y = BAR_H + wy - (view_y << 4);
    if (x >= TB_W && x < SCR_W && y >= BAR_H && y < SCR_H) fb[y * SCR_W + x] = pen;
}

static void ink_line(int x0, int y0, int x1, int y1, UBYTE pen)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        ink_pixel(x0, y0, pen); ink_pixel(x0 + 1, y0, pen);
        if (x0 == x1 && y0 == y1) break;
        { int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
    }
}

static void ink_walk(int draw)            /* draw = 0: mark the touched cells, 1: draw the lines */
{
    for (Ink *ink = sim->overlay; ink; ink = ink->next) {
        int x = ink->points[0].x, y = ink->points[0].y;
        UBYTE pen = ink->color == COLOR_WHITE ? PEN_TEXT : PEN_V_YELLOW;
        if (!draw) ink_mark(x, y); else ink_pixel(x, y, pen);
        for (int i = 1; i < ink->length; i++) {
            int nx = x + ink->points[i].x, ny = y + ink->points[i].y;
            if (draw) ink_line(x, y, nx, ny, pen);
            else {
                int n = (abs(nx - x) > abs(ny - y) ? abs(nx - x) : abs(ny - y)) / 8 + 1;
                for (int k = 0; k <= n; k++) ink_mark(x + (nx - x) * k / n, y + (ny - y) * k / n);
            }
            x = nx; y = ny;
        }
    }
}

static void ink_prepare(void)             /* before the tiles are drawn: which cells must be redrawn */
{
    int any = 0;
    memset(ink_cur, 0, sizeof ink_cur);
    if (sim->overlay) { ink_walk(0); any = 1; }
    if (any || ink_prev_any) {
        for (int row = 0; row < VIEW_ROWS; row++)
            for (int col = 0; col < VIEW_COLS; col++) {
                int mx = view_x + col, my = view_y + row;
                if (mx < WORLD_X && my < WORLD_Y && (ink_cur[my][mx] || ink_prev[my][mx])) redraw[row][col] = 1;
            }
    }
    memcpy(ink_prev, ink_cur, sizeof ink_prev);
    ink_prev_any = any;
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
enum { P_NOTICE, P_BUDGET, P_EVAL, P_GRAPH, P_QUERY, P_MAP, P_SOUND, NPANELS };
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
static void draw_sound(void) { gfx_target(scratch, SCRATCH_STRIDE); sound_ui_draw(scratch, SCRATCH_STRIDE); }
static void draw_graph(void)  { gfx_target(scratch, SCRATCH_STRIDE); graph_draw(scratch, SCRATCH_STRIDE); }
static void draw_map(void)    { map_blit(scratch, SCRATCH_STRIDE, 1, 1, view_x, view_y, VIEW_COLS, VIEW_ROWS); }
static void draw_query(void)
{
    static const char *const label[6] = { N_("Zone"), N_("Density"), N_("Value"), N_("Crime"), N_("Pollution"), N_("Growth") };
    for (int y = 0; y < QUERY_H; y++) memset(scratch + (QUERY_Y + y) * SCRATCH_STRIDE + QUERY_X, PEN_BAR, QUERY_W);
    gfx_target(scratch, SCRATCH_STRIDE);
    gfx_colors(PEN_TEXT, PEN_BAR);
    gfx_print(QUERY_X + 8, QUERY_Y + 18, T("Query zone status"));
    for (int i = 0; i < 6; i++) { gfx_print(QUERY_X + 8, QUERY_Y + 36 + i * 13, T(label[i])); gfx_print(QUERY_X + 8 + 88, QUERY_Y + 36 + i * 13, T(query_f[i])); }
}
static void close_query(void) { query_visible = 0; }
static void close_notice(void) { notice_close(); }
static void close_map(void) { map_visible = 0; }
static struct panel panels[NPANELS] = {
    { N_("Message"),             &notice_visible, &notice_dirty, NOTICE_X, NOTICE_Y, NOTICE_W, NOTICE_H, draw_notice, notice_click, close_notice, 0, 0, NULL },
    { N_("Budget"),              &budget_visible, &budget_dirty, BUDGET_X, BUDGET_Y, BUDGET_W, BUDGET_H, draw_budget, budget_click, budget_key_close, 0, 0, NULL },
    { "City Evaluation",     &eval_visible,   &eval_dirty,   EVAL_X, EVAL_Y, EVAL_W, EVAL_H, draw_eval, eval_click, eval_close, 0, 0, NULL },
    { N_("History"),             &graph_visible,  &graph_dirty,  GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, draw_graph, graph_click, graph_close, 0, 0, NULL },
    { N_("Query"),               &query_visible,  &query_dirty,  QUERY_X, QUERY_Y, QUERY_W, QUERY_H, draw_query, NULL, close_query, 1, 0, NULL },
    { N_("Overview map"),        &map_visible,    &map_panel_dirty, 0, 0, MAPWIN_W, MAPWIN_H, draw_map, NULL, close_map, 2, 1, NULL },
    { N_("Sound options"),       &sound_visible,  &sound_dirty,  SND_X, SND_Y, SND_W, SND_H, draw_sound, NULL, sound_ui_close, 0, 1, NULL },
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
    if (i < N_WINPOS && pref_wx[i] >= 0) { x = pref_wx[i]; y = pref_wy[i]; }      /* where the user left it */
    if (x > sw - p->w - 8) x = sw - p->w - 8;
    if (y > sh - p->h - 24) y = sh - p->h - 24;
    if (x < 0) x = 0;
    if (y < bar) y = bar;
    p->win = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)g_scr, WA_Left, x, WA_Top, y,
                            WA_InnerWidth, p->w, WA_InnerHeight, p->h, WA_Title, (ULONG)T(p->title),
                            WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE, WA_NewLookMenus, TRUE,
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
    if (i < N_WINPOS && i != P_NOTICE) { pref_wx[i] = p->win->LeftEdge; pref_wy[i] = p->win->TopEdge; }
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
#define DEM_W 100
#define DEM_X (SCR_W - DEM_W - 6)             /* at the right end of the status bar */
#define DEM_Y 0
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
        anim_hold = 12;                                          /* scrolling: no animation steps until it has been still for a moment */
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
    static int coast_drawn = -1;
    int coast_now = 0, coast_moved = coast_drawn != coast_tick;

    for (int row = 0; row < VIEW_ROWS; row++) {
        for (int col = 0; col < VIEW_COLS; col++) {
            int mx = view_x + col, my = view_y + row, tile = 0;
            if (mx < WORLD_X && my < WORLD_Y) {
                tile = (unsigned short)Map[mx][my];
                if ((tile & LOMASK) >= TILE_COUNT) tile -= TILE_COUNT;
                if (blink && (tile & ZONEBIT) && !(tile & PWRBIT)) tile = LIGHTNINGBOLT;
                else tile &= LOMASK;
            }
            if (world_anim && coast_has(tile)) { coast_now++; if (coast_moved) redraw[row][col] = 1; }
            else if (world_anim && wind_has(tile)) {                /* only trees a gust touches now or touched one step ago */
                int wx = mx << 4, wy = my << 4;
                coast_now++;
                if (coast_moved && (wind_active(wx, wy, coast_tick) || wind_active(wx, wy, coast_drawn))) redraw[row][col] = 1;
            }
            if (tile != shown[row][col]) { redraw[row][col] = 1; shown[row][col] = tile; }
        }
    }

    coast_drawn = coast_tick; coast_seen = coast_now;

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

    ink_prepare();

    /* draw the tiles that changed */
    for (int row = 0; row < VIEW_ROWS; row++) {
        for (int col = 0; col < VIEW_COLS; col++) {
            if (!redraw[row][col]) continue;
            draw_tile(col, row, shown[row][col]);
            stats.tiles++;
        }
    }
    if (sim->overlay) ink_walk(1);
    for (int i = 0; i < n_spr; i++) draw_sprite(&cur_spr[i], spr_px[i], spr_w[i], spr_h[i]);
    if (have_cursor) draw_cursor(&cur);
    for (int i = 0; i < n_spr; i++) prev_spr[i] = cur_spr[i];
    n_prev_spr = n_spr;
    prev_cursor = cur; have_prev_cursor = have_cursor;

    if (toolbar_dirty_ext) { toolbar_dirty = 1; tb_label_only = 0; toolbar_dirty_ext = 0; }
    gfx_target(fb, SCR_W);
    if (toolbar_dirty) {
        draw_toolbar();
    }
    {   /* status lines (top left) */
        char line[200];
        gfx_colors(PEN_TEXT, PEN_BAR);
        sprintf(line, " %-9s %-21s %s %-7ld %s %d%s", status_date, status_funds, T("Pop"), (long)CityPop, T("Speed"), (int)SimSpeed,
                SimSpeed ? "" : T(" (Pause)"));
        { char pad[200]; sprintf(pad, "%-66.66s", line); strcpy(line, pad); }     /* overwrite what an earlier, longer line (" (Pause)") left behind */
        if (strcmp(line, last_line1)) { gfx_print(4, 12, line); strcpy(last_line1, line); status_dirty = 1; }
        sprintf(line, " %-58.58s", status_msg);
        if (strcmp(line, last_line2)) { gfx_print(4, 27, line); strcpy(last_line2, line); status_dirty = 1; }
    }
    if (demand_dirty) {
        draw_demand();
        gfx_colors(PEN_TEXT, PEN_BAR);
        gfx_print(DEM_X + 10, DEM_Y + 30, "R"); gfx_print(DEM_X + 46, DEM_Y + 30, "C"); gfx_print(DEM_X + 82, DEM_Y + 30, "I");
    }
    { long el = elapsed_us();
      stats.render_us += el;
      if (coast_moved) anim_slow = el > 60000 ? 1 : el < 30000 ? 0 : anim_slow;      /* an animation step that takes long: slow it down (with some hysteresis) */
    }

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
        if (toolbar_dirty) blit(rp, 0, BAR_H, TB_W, tb_label_only ? 56 : SCR_H - BAR_H);
        if (demand_dirty) blit(rp, DEM_X, DEM_Y, DEM_W, DEM_H);
        if (status_dirty) blit(rp, 0, 0, DEM_X, BAR_H);
    }

    toolbar_dirty = 0; tb_label_only = 0; status_dirty = 0; demand_dirty = 0;

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
       CMD_AUTOBULLDOZE = 20, CMD_AUTOBUDGET, CMD_DISASTERS, CMD_SOUND, CMD_ASKMODE, CMD_AUTOGO, CMD_ANIM, CMD_MSGS, CMD_NOTICES, CMD_OVERLAY, CMD_LEVEL = 60, CMD_SPEAK = 65, CMD_STARTMENU = 66, CMD_CLASSIC = 67, CMD_LANG = 70, CMD_AMBIENT = 75, CMD_SOUNDWIN = 76, CMD_WORLDANIM = 77,
       CMD_ABOUT = 44, CMD_HELP = 45, CMD_SCEN = 200, CMD_BUDGET = 40, CMD_MAP = 41, CMD_EVAL = 42, CMD_GRAPHS = 43, CMD_MAPMODE = 100,
       CMD_FIRE = 30, CMD_FLOOD, CMD_TORNADO, CMD_QUAKE, CMD_MONSTER, CMD_MELTDOWN };

static struct NewMenu newmenu[80];                      /* the menus have about 48 entries */
static int nmenu;
static unsigned mi_excl;                       /* mutual-exclude mask for the next item (GadTools: bit n = item n of the same menu) */
static void mi(UBYTE type, const char *label, const char *key, UWORD flags, int id)
{
    if (nmenu >= (int)(sizeof newmenu / sizeof newmenu[0]) - 1) return;     /* never overrun the table */
    struct NewMenu *n = &newmenu[nmenu++];
    n->nm_Type = type; n->nm_Label = (STRPTR)label; n->nm_CommKey = (STRPTR)key;
    n->nm_Flags = flags; n->nm_MutualExclude = mi_excl; mi_excl = 0; n->nm_UserData = (APTR)(long)id;
}

static struct Menu *make_menus(struct Screen *scr, APTR *vi_out)
{
    APTR vi = GetVisualInfoA(scr, NULL);
    struct Menu *menu;
    nmenu = 0;
    mi(NM_TITLE, T("Project"), NULL, 0, 0);
    mi(NM_ITEM, T("Help..."), NULL, 0, CMD_HELP);
    mi(NM_ITEM, T("About Micropolis..."), NULL, 0, CMD_ABOUT);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    mi(NM_ITEM, T("New City"), NULL, 0, CMD_NEW);
    mi(NM_ITEM, T("Load City..."), "L", 0, CMD_LOAD);
    mi(NM_ITEM, T("Save City"), "S", 0, CMD_SAVE);
    mi(NM_ITEM, T("Save City As..."), NULL, 0, CMD_SAVEAS);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    mi(NM_ITEM, T("Quit"), "Q", 0, CMD_QUIT);
    mi(NM_TITLE, T("Scenarios"), NULL, 0, 0);
    {   static const char *const scen[8] = { N_("Dullsville 1900"), N_("San Francisco 1906"), N_("Hamburg 1944"), N_("Bern 1965"),
                                              N_("Tokyo 1957"), N_("Detroit 1972"), N_("Boston 2010"), N_("Rio de Janeiro 2047") };
        for (int i = 0; i < 8; i++) mi(NM_ITEM, T(scen[i]), NULL, 0, CMD_SCEN + i + 1);
    }
    mi(NM_TITLE, T("Speed"), NULL, 0, 0);
    mi(NM_ITEM, T("Pause"), "0", 0, CMD_PAUSE);
    mi(NM_ITEM, T("Slow"), "1", 0, CMD_SLOW);
    mi(NM_ITEM, T("Medium"), "2", 0, CMD_MEDIUM);
    mi(NM_ITEM, T("Fast"), "3", 0, CMD_FAST);
    mi(NM_TITLE, T("Options"), NULL, 0, 0);
    mi(NM_ITEM, T("Auto Bulldoze"), NULL, CHECKIT | MENUTOGGLE | (autoBulldoze ? CHECKED : 0), CMD_AUTOBULLDOZE);
    mi(NM_ITEM, T("Auto Budget"), NULL, CHECKIT | MENUTOGGLE | (autoBudget ? CHECKED : 0), CMD_AUTOBUDGET);
    mi(NM_ITEM, T("Disasters"), NULL, CHECKIT | MENUTOGGLE | (NoDisasters ? 0 : CHECKED), CMD_DISASTERS);
    mi(NM_ITEM, T("Sound options..."), NULL, 0, CMD_SOUNDWIN);
    mi(NM_ITEM, T("Auto Goto (jump to events)"), NULL, CHECKIT | MENUTOGGLE | (autoGo ? CHECKED : 0), CMD_AUTOGO);
    mi(NM_ITEM, T("City animation"), NULL, CHECKIT | MENUTOGGLE | (DoAnimation ? CHECKED : 0), CMD_ANIM);
    mi(NM_ITEM, T("World animation"), NULL, CHECKIT | MENUTOGGLE | (world_anim ? CHECKED : 0), CMD_WORLDANIM);
    mi(NM_ITEM, T("Messages"), NULL, CHECKIT | MENUTOGGLE | (DoMessages ? CHECKED : 0), CMD_MSGS);
    mi(NM_ITEM, T("Start menu at launch"), NULL, CHECKIT | MENUTOGGLE | (start_menu ? CHECKED : 0), CMD_STARTMENU);
    mi(NM_ITEM, T("Classic tool icons"), NULL, CHECKIT | MENUTOGGLE | (classic_icons ? CHECKED : 0), CMD_CLASSIC);
    mi(NM_ITEM, T("Message windows"), NULL, CHECKIT | MENUTOGGLE | (DoNotices ? CHECKED : 0), CMD_NOTICES);
    mi(NM_ITEM, T("Ask for screen mode at start"), NULL, CHECKIT | MENUTOGGLE | (pref_ask ? CHECKED : 0), CMD_ASKMODE);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    mi(NM_ITEM, T("Level"), NULL, 0, 0);                     /* level and language are sub menus: nine menu titles do not fit into 640 pixels */
    mi_excl = 6; mi(NM_SUB, T("Easy ($20,000 start)"),   NULL, CHECKIT | (pref_level == 0 ? CHECKED : 0), CMD_LEVEL + 0);
    mi_excl = 5; mi(NM_SUB, T("Medium ($10,000 start)"), NULL, CHECKIT | (pref_level == 1 ? CHECKED : 0), CMD_LEVEL + 1);
    mi_excl = 3; mi(NM_SUB, T("Hard ($5,000 start)"),    NULL, CHECKIT | (pref_level == 2 ? CHECKED : 0), CMD_LEVEL + 2);
    mi(NM_ITEM, T("Language"), NULL, 0, 0);
    {   static const char *const code[4] = { "", "english", "deutsch", "polski" };
        const char *label[4] = { T("Automatic (system setting)"), "English", "Deutsch", "Polski" };
        static const int ex[4] = { 14, 13, 11, 7 };                  /* the other three items of the group */
        for (int i = 0; i < 4; i++) { mi_excl = ex[i]; mi(NM_SUB, label[i], NULL, CHECKIT | (strcmp(pref_lang, code[i]) == 0 ? CHECKED : 0), CMD_LANG + i); }
    }
    mi(NM_TITLE, T("Windows"), NULL, 0, 0);
    mi(NM_ITEM, T("Budget..."), "B", 0, CMD_BUDGET);
    mi(NM_ITEM, T("Evaluation..."), "E", 0, CMD_EVAL);
    mi(NM_ITEM, T("Graphs..."), "G", 0, CMD_GRAPHS);
    mi(NM_TITLE, T("Map"), NULL, 0, 0);
    mi(NM_ITEM, T("Map window on/off"), NULL, 0, CMD_MAP);   /* Amiga+M belongs to the system: plain M instead */
    mi(NM_ITEM, T("Next overlay"), "C", 0, CMD_OVERLAY);
    mi(NM_ITEM, (const char *)NM_BARLABEL, NULL, 0, 0);
    for (int i = 0; i < MAP_MODES; i++) mi(NM_ITEM, T(map_names[i]), NULL, 0, CMD_MAPMODE + i);
    mi(NM_TITLE, T("Disasters"), NULL, 0, 0);
    mi(NM_ITEM, T("Fire"), NULL, 0, CMD_FIRE);
    mi(NM_ITEM, T("Flood"), NULL, 0, CMD_FLOOD);
    mi(NM_ITEM, T("Tornado"), NULL, 0, CMD_TORNADO);
    mi(NM_ITEM, T("Earthquake"), NULL, 0, CMD_QUAKE);
    mi(NM_ITEM, T("Monster"), NULL, 0, CMD_MONSTER);
    mi(NM_ITEM, T("Meltdown"), NULL, 0, CMD_MELTDOWN);
    mi(NM_END, NULL, NULL, 0, 0);
    *vi_out = vi;
    if (!vi) return NULL;
    menu = CreateMenusA(newmenu, NULL);
    if (menu && !LayoutMenus(menu, vi, GTMN_NewLookMenus, TRUE, TAG_DONE)) { FreeMenus(menu); menu = NULL; }   /* new look: the menus use the screen's pens (grey, 3D), not the 1.3 black-and-colour 1 look */
    if (menu) {                                               /* every title must be reachable: the last one has to end inside the screen */
        int right = 0; char lb[80];
        for (struct Menu *m = menu; m; m = m->NextMenu) if (m->LeftEdge + m->Width > right) right = m->LeftEdge + m->Width;
        sprintf(lb, "menus: right edge %d of %d pixels", right, (int)scr->Width); logmsg(lb);
    }
    return menu;
}

/* file requester (asl.library); returns 1 and a full path in out, 0 if cancelled */
static int ask_file(struct Window *win, int save, char *out, size_t n)
{
    int ok = 0;
    if (!AslBase) return 0;
    struct FileRequester *fr = AllocAslRequestTags(ASL_FileRequest,
        ASLFR_Window, (ULONG)win, ASLFR_TitleText, (ULONG)(save ? T("Save City") : T("Load City")),
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

/* What is on the screen decides the background sound. Water and built-up area each get their sound when they take up
 * enough of the picture (silent below 15 %, full at 55-60 %); the nature sound fills in whatever neither of them
 * dominates, so open land, forest, a few roads or a thin town all sound like nature. Roads, rails and wires count half
 * as built-up area (a road through the woods is still woods); the middle of the picture counts double. */
#define AMB_MAX 22
static void ambient_update(void)
{
    long water = 0, built = 0, infra = 0, total;
    int sea_dom, city_dom, nat;
    if (!ambient_on || !UserSoundOn) { ambient_set(0, 0, 0); return; }          /* it keeps playing while the game is paused */
    for (int row = 0; row < VIEW_ROWS; row++) {
        for (int col = 0; col < VIEW_COLS; col++) {
            int mx = view_x + col, my = view_y + row, w, t;
            if (mx < 0 || my < 0 || mx >= WORLD_X || my >= WORLD_Y) continue;
            w = (col > VIEW_COLS / 4 && col < VIEW_COLS * 3 / 4 && row > VIEW_ROWS / 4 && row < VIEW_ROWS * 3 / 4) ? 2 : 1;
            t = (unsigned short)Map[mx][my] & LOMASK;
            if (t >= 2 && t <= 20) water += w;                 /* river, shore, channel */
            else if (t >= 240) built += w;                      /* zones, buildings, plants, stations */
            else if (t >= 64) infra += w;                       /* roads, rails, wires */
            /* everything else (bare land, trees, woods, rubble) is open land: it is what is left over */
        }
    }
    {   long vis = 0;                                           /* all counted tiles, weighted */
        for (int row = 0; row < VIEW_ROWS; row++)
            for (int col = 0; col < VIEW_COLS; col++) {
                int mx = view_x + col, my = view_y + row;
                if (mx < 0 || my < 0 || mx >= WORLD_X || my >= WORLD_Y) continue;
                vis += (col > VIEW_COLS / 4 && col < VIEW_COLS * 3 / 4 && row > VIEW_ROWS / 4 && row < VIEW_ROWS * 3 / 4) ? 2 : 1;
            }
        total = vis ? vis : 1;
    }
    {   long ws = water * 100 / total, cs = (built * 100 + infra * 50) / total;      /* shares in percent */
        long sd = (ws - 15) * 100 / 40, cd = (cs - 15) * 100 / 45;                  /* how strongly each one dominates, 0..100 */
        sea_dom = (int)(sd < 0 ? 0 : sd > 100 ? 100 : sd);
        city_dom = (int)(cd < 0 ? 0 : cd > 100 ? 100 : cd);
    }
    nat = 100 - (sea_dom > city_dom ? sea_dom : city_dom);                          /* nature fills in what neither dominates */
    {   int mx = AMB_MAX * 2 * sound_amb_vol / 100;                 /* slider 50 % = the old default level */
        ambient_set(sea_dom * mx / 100, city_dom * mx / 100, nat * mx / 100);
    }
    if (debug_log) { static int last[3] = { -1, -1, -1 }; if (last[0] != sea_dom || last[1] != city_dom || last[2] != nat) {
        char b[96]; sprintf(b, "ambient: sea %d city %d nature %d (water %ld%%, built %ld%%, infra %ld%%)", sea_dom, city_dom, nat, water * 100 / total, built * 100 / total, infra * 100 / total);
        logmsg(b); last[0] = sea_dom; last[1] = city_dom; last[2] = nat; } }
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
    char txt[800], cpu[40] = "68000", fpu[24] = "", modeline[60], snd[60];
    sound_status(snd);
    if (!UserSoundOn) strcpy(snd, T("Sound: switched off (Options menu)"));
    sprintf(modeline, T("Screen 0x%08lx, %dx%d, %d bit"), (unsigned long)scr_mode_shown, scr_w, scr_h, scr_depth_shown);
    UWORD af = SysBase->AttnFlags;
    strcpy(fpu, T("no FPU"));
    if (af & AFF_68010) strcpy(cpu, "68010");
    if (af & AFF_68020) strcpy(cpu, "68020");
    if (af & AFF_68030) strcpy(cpu, "68030");
    if (af & AFF_68040) strcpy(cpu, "68040");
    if (af & 0x80) strcpy(cpu, "68060");                 /* AFF_68060 */
    if (af & (AFF_68881 | AFF_68882)) strcpy(fpu, (af & AFF_68882) ? "68882" : "68881");
    if (af & AFF_FPU40) strcpy(fpu, "FPU");
    {   char *q = txt;
        q += sprintf(q, "%s\n", T("Micropolis for AmigaOS 3"));
        q += sprintf(q, T("Version %s (%s)"), VERSION, VER_DATE); *q++ = '\n';
        q += sprintf(q, T("Build  %s  %s"), BUILD_TIME, BUILD_REV); *q++ = '\n'; *q++ = '\n';
        q += sprintf(q, T("CPU %s, %s"), cpu, fpu); *q++ = '\n';
        q += sprintf(q, "%s\n", modeline);
        q += sprintf(q, T("Memory: %ld KB Chip, %ld KB Fast free"), (long)(AvailMem(MEMF_CHIP) / 1024), (long)(AvailMem(MEMF_FAST) / 1024)); *q++ = '\n';
        q += sprintf(q, "%s\n\n", snd);
        q += sprintf(q, "%s\n", T("Simulation and graphics from Micropolis, the open-source SimCity (GPL v3, (c) Electronic Arts). Original design Will Wright, Unix version Don Hopkins."));
        q += sprintf(q, "%s", T("Translations: see the catalogs in the Catalogs drawer."));
    }
    notice_show_text(T("About Micropolis"), txt);
    notice_dirty = 1;
}

/* returns 0 to quit */
static void fail(const char *fmt, ...);
static void guide_poll(void)                                 /* AmigaGuide talks to us through messages */
{
    struct AmigaGuideMsg *gm;
    if (!guide_handle) return;
    while ((gm = GetAmigaGuideMsg(guide_handle))) {
        if (gm->agm_Type == ToolStatusID && gm->agm_Pri_Ret) { char lb[60]; sprintf(lb, "help: error %ld", (long)gm->agm_Pri_Ret); logmsg(lb); }
        ReplyAmigaGuideMsg(gm);
    }
}

static void show_help(void)                                  /* AmigaGuide document next to the program */
{
    struct NewAmigaGuide nag;
    if (!AmigaGuideBase) { fail(T("amigaguide.library is not available.")); return; }
    if (guide_handle) { CloseAmigaGuide(guide_handle); guide_handle = NULL; }
    memset(&nag, 0, sizeof nag);
    {   BPTR pd = GetProgramDir();                          /* the guide's process does not know our PROGDIR: */
        if (pd && NameFromLock(pd, (STRPTR)guide_path, sizeof guide_path - 20) && AddPart((STRPTR)guide_path, (STRPTR)"Micropolis.guide", sizeof guide_path)) ;
        else strcpy(guide_path, "PROGDIR:Micropolis.guide");
    }
    {   char loc[240];                                      /* a translated guide lives next to the catalog: Catalogs/<language>/Micropolis.guide */
        BPTR l;
        strcpy(loc, guide_path);
        char *sl = strrchr(loc, '/'); if (!sl) sl = strrchr(loc, ':');
        if (sl && strcmp(l10n_language(), "english")) {
            sprintf(sl + 1, "Catalogs/%s/Micropolis.guide", l10n_language());
            if ((l = Lock((CONST_STRPTR)loc, ACCESS_READ))) { UnLock(l); strcpy(guide_path, loc); }
        }
    }
    nag.nag_Name = (STRPTR)guide_path;
    nag.nag_Screen = g_scr;
    nag.nag_BaseName = (STRPTR)"Micropolis";
    guide_handle = OpenAmigaGuideA(&nag, NULL);
    { char lb[300]; sprintf(lb, "help: path=%s scr=%p err=%ld", guide_path, (void *)g_scr, (long)IoErr()); logmsg(lb); }
    logmsg(guide_handle ? "help: guide opened" : "help: guide open failed");
    if (!guide_handle) fail(T("Cannot open Micropolis.guide."));
}

static void save_city(struct Window *win, const char *path)
{
    char name[256];
    strncpy(name, path, sizeof name - 1); name[sizeof name - 1] = 0;     /* SaveCityAs takes ownership of its own copy and cuts the name it is given */
    save_state = 0;
    SaveCityAs(name);
    if (save_state == 1) { sprintf(status_msg, T("Saved %s"), path); clean_time = (long)CityTime; tool_used = 0; }
    else {
        sprintf(status_msg, T("Could not save %s"), path);
        fail(T("Cannot save the city:\n%s"), save_state == 2 ? save_msg : T("unknown error"));
    }
}

static void menu_sync(void)                                  /* make the check marks of the menu match the game's variables (after loading a city) */
{
    struct Window *w[1 + NPANELS]; int n = 0;
    if (!g_menu) return;
    if (main_window_ptr) w[n++] = main_window_ptr;
    for (int i = 0; i < NPANELS; i++) if (panels[i].win) w[n++] = panels[i].win;
    for (int i = 0; i < n; i++) ClearMenuStrip(w[i]);
    for (struct Menu *m = g_menu; m; m = m->NextMenu)
        for (struct MenuItem *top = m->FirstItem; top; top = top->NextItem)
         for (struct MenuItem *it = top; it; it = (it == top) ? top->SubItem : it->NextItem) {
            int id = (int)(long)GTMENUITEM_USERDATA(it), want = -1;
            switch (id) {
            case CMD_AUTOBULLDOZE: want = autoBulldoze; break;
            case CMD_AUTOBUDGET: want = autoBudget; break;
            case CMD_DISASTERS: want = !NoDisasters; break;
            case CMD_SOUND: want = UserSoundOn; break;
            case CMD_AUTOGO: want = autoGo; break;
            case CMD_ANIM: want = DoAnimation; break;
            case CMD_WORLDANIM: want = world_anim; break;
            case CMD_MSGS: want = DoMessages; break;
            case CMD_NOTICES: want = DoNotices; break;
            default:
                if (id >= CMD_LEVEL && id < CMD_LEVEL + 3) want = (GameLevel == id - CMD_LEVEL);
            }
            if (want >= 0) { if (want) it->Flags |= CHECKED; else it->Flags &= ~CHECKED; }
        }
    for (int i = 0; i < n; i++) ResetMenuStrip(w[i], g_menu);
}

static int easy(struct Window *win, const char *text, const char *gadgets);
static int ask_quit(struct Window *win);
static int do_command(int id, struct Window *win, struct MenuItem *item)
{
    char path[256];
    int checked = item && (item->Flags & CHECKED);
    if (id >= CMD_MAPMODE && id < CMD_MAPMODE + MAP_MODES) {
        map_mode = id - CMD_MAPMODE; map_visible = 1; map_dirty = 1; map_panel_dirty = 1;
        sprintf(status_msg, T("Map: %s"), T(map_names[map_mode]));
        return 1;
    }
    if (id > CMD_SCEN && id <= CMD_SCEN + 8) {            /* scenario: load it and show its story, as the original does */
        EraseOverlay(); LoadScenario((short)(id - CMD_SCEN)); clean_time = (long)CityTime; tool_used = 0;
        recenter();
        notice_show(id - CMD_SCEN); notice_dirty = 1;
        return 1;
    }
    if (id >= CMD_LANG && id < CMD_LANG + 4) {              /* language: used from the next start */
        static const char *const code[4] = { "", "english", "deutsch", "polski" };
        strcpy(pref_lang, code[id - CMD_LANG]);
        prefs_save();
        easy(win, T("The language changes at the next start of Micropolis."), T("OK"));
        return 1;
    }
    if (id >= CMD_LEVEL && id < CMD_LEVEL + 3) {          /* difficulty: the start funds of the next new city, and the evaluation */
        pref_level = id - CMD_LEVEL;
        SetGameLevel((short)pref_level);
        prefs_save();
        sprintf(status_msg, T("Level: %s"), pref_level == 0 ? T("Easy") : pref_level == 1 ? T("Medium") : T("Hard"));
        return 1;
    }
    switch (id) {
    case CMD_HELP: show_help(); break;
    case CMD_ABOUT: show_about(); break;
    case CMD_NEW:
        EraseOverlay(); GenerateNewCity(); CityTax = 7; SetGameLevelFunds((short)pref_level); setSpeed(2); clean_time = (long)CityTime; tool_used = 0; recenter();
        sprintf(status_msg, T("New city generated.")); break;
    case CMD_LOAD:
        if (ask_file(win, 0, path, sizeof path)) {
            if (LoadCity(path)) { EraseOverlay(); recenter(); clean_time = (long)CityTime; tool_used = 0; sprintf(status_msg, T("Loaded %s"), path); }
            else sprintf(status_msg, T("Could not load %s"), path);
        }
        break;
    case CMD_SAVE:
        if (CityFileName) { strncpy(path, CityFileName, sizeof path - 1); path[sizeof path - 1] = 0; save_city(win, path); break; }
        /* no name yet: fall through to "Save As" */
    case CMD_SAVEAS:
        if (ask_file(win, 1, path, sizeof path)) {
            size_t l = strlen(path);
            if (l < 4 || strcasecmp(path + l - 4, ".cty")) strncat(path, ".cty", sizeof path - l - 1);   /* the file requester's pattern is #?.cty */
            save_city(win, path);
        }
        break;
    case CMD_QUIT: return ask_quit(win) ? 0 : 1;
    case CMD_PAUSE: setSpeed(0); break;
    case CMD_SLOW: setSpeed(1); break;
    case CMD_MEDIUM: setSpeed(2); break;
    case CMD_FAST: setSpeed(3); break;
    case CMD_AUTOBULLDOZE: autoBulldoze = checked; break;
    case CMD_AUTOBUDGET: autoBudget = checked; break;
    case CMD_DISASTERS: NoDisasters = !checked; break;
    case CMD_SOUND: UserSoundOn = checked; break;
    case CMD_AUTOGO: autoGo = checked; MustUpdateOptions = 1; break;
    case CMD_ANIM: DoAnimation = checked; MustUpdateOptions = 1; full_redraw = 1; break;
    case CMD_WORLDANIM: world_anim = checked; full_redraw = 1; break;
    case CMD_MSGS: DoMessages = checked; MustUpdateOptions = 1; break;
    case CMD_AMBIENT: ambient_on = checked; break;
    case CMD_CLASSIC: classic_icons = checked; toolbar_dirty = 1; tb_label_only = 0; break;
    case CMD_STARTMENU: start_menu = checked; break;
    case CMD_SPEAK: speak_tools = checked; break;
    case CMD_NOTICES: DoNotices = checked; MustUpdateOptions = 1; break;
    case CMD_OVERLAY: map_mode = (map_mode + 1) % MAP_MODES; map_visible = 1; map_dirty = 1; map_panel_dirty = 1; sprintf(status_msg, T("Map: %s"), T(map_names[map_mode])); break;
    case CMD_ASKMODE:
        pref_ask = checked;
        if (!pref_ask) { pref_mode = (ULONG)scr_mode_shown; pref_w = scr_w; pref_h = scr_h; pref_depth = scr_depth_shown; }   /* keep using the current mode */
        prefs_save();
        sprintf(status_msg, pref_ask ? T("The screen mode will be asked at the next start.") : T("This screen mode is used from now on."));
        break;
    case CMD_BUDGET: DoBudgetFromMenu(); break;
    case CMD_SOUNDWIN: open_info_panel(P_SOUND); break;
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


static int easy(struct Window *win, const char *text, const char *gadgets)   /* 1..n for the buttons left to right, the last = 0 */
{
    struct EasyStruct es;
    es.es_StructSize = sizeof(struct EasyStruct); es.es_Flags = 0;
    es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)text; es.es_GadgetFormat = (CONST_STRPTR)gadgets;
    return (int)EasyRequestArgs(win, &es, NULL, NULL);
}

static int ask_quit(struct Window *win)                 /* 1 = quit now, 0 = stay */
{
    char path[256];
    int r;
    if (!tool_used && (long)CityTime == clean_time) return 1;       /* nothing to lose */
    r = easy(win, T("Quit Micropolis?\nThe city has changed since it was last saved."), T("Save and quit|Quit|Cancel"));
    if (r == 2) return 1;
    if (r != 1) return 0;
    if (CityFileName) { strncpy(path, CityFileName, sizeof path - 1); path[sizeof path - 1] = 0; }
    else if (!ask_file(win, 1, path, sizeof path)) return 0;
    else { size_t l = strlen(path); if (l < 4 || strcasecmp(path + l - 4, ".cty")) strncat(path, ".cty", sizeof path - l - 1); }
    save_city(win, path);
    return save_state == 1;
}

static int pick_scenario(struct Window *win)             /* 1..8, 0 = back */
{
    static const char *const nm[8] = { N_("Dullsville 1900"), N_("San Francisco 1906"), N_("Hamburg 1944"), N_("Bern 1965"), N_("Tokyo 1957"), N_("Detroit 1972"), N_("Boston 2010"), N_("Rio de Janeiro 2047") };
    char g[160];
    for (int page = 0; ; page = (page + 1) % 3) {
        int r;
        if (page == 0) sprintf(g, "%s|%s|%s|%s", T(nm[0]), T(nm[1]), T(nm[2]), T("More..."));
        else if (page == 1) sprintf(g, "%s|%s|%s|%s", T(nm[3]), T(nm[4]), T(nm[5]), T("More..."));
        else sprintf(g, "%s|%s|%s", T(nm[6]), T(nm[7]), T("Back"));
        r = easy(win, T("Choose a scenario:"), g);
        if (page < 2) { if (r >= 1 && r <= 3) return page * 3 + r; }
        else { if (r == 1) return 7; if (r == 2) return 8; return 0; }
    }
}

static void start_choice(struct Window *win)             /* what to start with */
{
    char path[256];
    for (;;) {
        int r = easy(win, T("Welcome to Micropolis!\nWhat do you want to start with?"), T("New city|Load city...|Scenario...|Current city"));
        if (r == 1) { do_command(CMD_NEW, win, NULL); return; }
        if (r == 2) { if (ask_file(win, 0, path, sizeof path) && LoadCity(path)) { EraseOverlay(); recenter(); sprintf(status_msg, T("Loaded %s"), path); return; } }
        else if (r == 3) { int sc = pick_scenario(win); if (sc) { do_command(CMD_SCEN + sc, win, NULL); return; } }
        else return;                                       /* Current city (the default one) */
    }
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
        es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)"%s"; es.es_GadgetFormat = (CONST_STRPTR)T("OK");
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
                if ((v = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes, (CONST_STRPTR)"LANGUAGE"))) { strncpy(arg_lang, (char *)v, sizeof arg_lang - 1); }
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
        else if (!strncasecmp(argv[i], "lang=", 5)) strncpy(arg_lang, argv[i] + 5, sizeof arg_lang - 1);
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
            ASLSM_TitleText, (ULONG)T("Micropolis: choose an RTG screen mode (640x480 to 800x600)"),
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
                es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)T("That is not a usable RTG mode.\nPlease choose one of the listed RTG modes\n(640x480 up to 800x600).");
                es.es_GadgetFormat = (CONST_STRPTR)T("Choose again");
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
    if (argc > 1 && strcasecmp(argv[1], "debug") && strncasecmp(argv[1], "depth=", 6) && strncasecmp(argv[1], "size=", 5) && strcasecmp(argv[1], "ask") && strncasecmp(argv[1], "lang=", 5)) {
        city = argv[1];                                  /* given on the command line: relative to the current directory */
    } else {                                             /* default / CITY tooltype: relative to the program's drawer */
        const char *c = tt_city[0] ? tt_city : "cities/haight.cty";
        if (strchr(c, ':')) strncpy(city_buf, c, sizeof city_buf - 1);
        else sprintf(city_buf, "PROGDIR:%.180s", c);
        city = city_buf;
    }
    test_mode = argc > 2 && !strcmp(argv[2], "test");
    l10n_init(arg_lang[0] ? arg_lang : pref_lang);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    KeymapBase = OpenLibrary((CONST_STRPTR)"keymap.library", 36);
    AmigaGuideBase = OpenLibrary((CONST_STRPTR)"amigaguide.library", 34);
    { char b[80]; struct Task *t = FindTask(NULL);
      sprintf(b, "stack: %ld bytes", (long)((char *)t->tc_SPUpper - (char *)t->tc_SPLower)); logmsg(b); }
    logmsg("start");
    if (!CyberGfxBase && !test_mode) {
        fail(T("cybergraphics.library (version 40) not found.\nMicropolis needs an RTG graphics card with\nPicasso96 or CyberGraphX installed and running."));
        goto out;
    }
    logmsg("libs ok");
    if (!load_assets()) {
        fail(T("Cannot find the program data (res drawer).\nPlease keep the Micropolis program, its res and\ncities drawers together in one drawer."));
        goto out;
    }

    logmsg("assets ok");
    HomeDir = ""; ResourceDir = "PROGDIR:res"; HostName = "amiga";
    sim = MakeNewSim();
    logmsg("sim made");
    sim_init();
    logmsg("sim_init done");
    if (!LoadCity((char *)city)) { fail(T("Cannot load the city file '%s'."), city); goto out; }
    logmsg("city loaded");
    clean_time = (long)CityTime; tool_used = 0;
    if (CityTax == 0) CityTax = 7;                        /* the default city (haight.cty) was saved with 0 %; the original starts with 7 % */
    sprintf(status_msg, "Micropolis %s (%s)", VERSION, BUILD_REV);
    view_x = WORLD_X / 2 - VIEW_COLS / 2;
    view_y = WORLD_Y / 2 - VIEW_ROWS / 2;
    clamp_view();

    if (pref_fxvol >= 0) sound_fx_vol = pref_fxvol;
    if (pref_ambvol >= 0) sound_amb_vol = pref_ambvol;
    for (int o = 0; o < N_OPT; o++) if (pref_opt[o] >= 0) opt_set(o, pref_opt[o]);     /* saved options (before the menus are built) */
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
                es.es_Title = (CONST_STRPTR)"Micropolis"; es.es_TextFormat = (CONST_STRPTR)T("Always use this screen mode (%ldx%ld, %ld bit)?\nOptions > \"Ask for screen mode at start\" turns the question on again.");
                es.es_GadgetFormat = (CONST_STRPTR)T("Always use it|Ask me next time");
                args[0] = (ULONG)w; args[1] = (ULONG)h; args[2] = (ULONG)d;
                pref_ask = EasyRequestArgs(NULL, &es, NULL, (APTR)args) ? 0 : 1;
                pref_mode = id; pref_w = w; pref_h = h; pref_depth = d;
                prefs_save();
            }
        }
    }
    if (id == INVALID_ID) {
        fail(T("No suitable RTG screen mode found.\nMicropolis needs a mode of at least 640x480 pixels\nwith 8, 15, 16, 24 or 32 bit colour."));
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
    if (!scr) { fail(T("Cannot open the RTG screen (mode 0x%08lx, %d bit)."), (unsigned long)id, depth); goto out; }
    g_scr = scr;
    logmsg("screen open");
    {   struct DrawInfo *dri = GetScreenDrawInfo(scr);                 /* which pens will the menus use? (they are inverted when highlighted) */
        if (dri) {
            char b[160]; int n = dri->dri_NumPens < 12 ? dri->dri_NumPens : 12;
            strcpy(b, "pens:");
            for (int i = 0; i < n; i++) sprintf(b + strlen(b), " %d", (int)dri->dri_Pens[i]);
            logmsg(b);
            FreeScreenDrawInfo(scr, dri);
        }
    }
    {   /* the game picture fills the screen below the title bar */
        int top = scr->BarHeight + 1;
        scr_w = scr->Width > MAX_W ? MAX_W : scr->Width;
        scr_h = scr->Height - top > MAX_H ? MAX_H : scr->Height - top;
        view_cols = (scr_w - TB_W) / 16; view_rows = (scr_h - BAR_H) / 16;
        fb = AllocVec(scr_w * scr_h, MEMF_ANY | MEMF_CLEAR);
        scratch = AllocVec(SCRATCH_STRIDE * MAX_H, MEMF_ANY | MEMF_CLEAR);
        if (!fb || !scratch) { fail(T("Not enough memory for a %dx%d screen."), scr_w, scr_h); goto out; }
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
        if (!idcmp_port) { fail(T("Cannot create a message port.")); goto out; }
        win = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 0, WA_Top, top, WA_Width, scr_w,
                             WA_Height, scr_h, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE, WA_NewLookMenus, TRUE,
                             WA_ReportMouse, TRUE, WA_SimpleRefresh, TRUE, WA_IDCMP, 0, TAG_DONE);
        if (!win) { fail(T("Cannot open the game window.")); goto out; }
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
    editor.tool_state = queryState; editor.tool_state_save = -1;
    SetGameLevel((short)pref_level);
    setSpeed(2);                                          /* medium: the fast speed was too quick for most cities */
    for (int ai = 1; ai < argc; ai++) if (!strncmp(argv[ai], "quit=", 5)) dbg_quit_loops = atoi(argv[ai] + 5) * 50;
    for (int di = 3; di < argc; di++) {                          /* debug options, any number, any order (arguments 3 and up) */
        const char *a = argv[di];
        if (!strncmp(a, "scroll=", 7)) { dbg_scroll = atoi(a + 7); setSpeed(0); }   /* debug: scroll test */
        if (!strncmp(a, "jump=", 5)) { int jx = 0, jy = 0; sscanf(a + 5, "%d,%d", &jx, &jy); view_x = jx; view_y = jy; clamp_view(); setSpeed(0); }
        if (!strncmp(a, "sound=", 6)) sound_play(a + 6);               /* debug: play a sound at start */
        if (!strncmp(a, "scen=", 5)) { int n = atoi(a + 5); LoadScenario((short)n); recenter(); notice_show(n); }   /* debug: start a scenario */
        if (!strncmp(a, "query=", 6)) { int qx = 60, qy = 50; sscanf(a + 6, "%d,%d", &qx, &qy); doZoneStatus((short)qx, (short)qy); }   /* debug */
        if (!strcmp(a, "modes")) {               /* debug: which display modes pass the filter */
            ULONG m = INVALID_ID; int n = 0;
            while ((m = NextDisplayInfo(m)) != INVALID_ID) {
                if (mode_ok(m)) { char lb[90]; sprintf(lb, "mode ok: 0x%08lx %ldx%ld %ld bit", (unsigned long)m, (long)GetCyberIDAttr(CYBRIDATTR_WIDTH, m), (long)GetCyberIDAttr(CYBRIDATTR_HEIGHT, m), (long)GetCyberIDAttr(CYBRIDATTR_DEPTH, m)); logmsg(lb); n++; }
            }
            { char lb[40]; sprintf(lb, "modes accepted: %d", n); logmsg(lb); }
        }
        if (!strcmp(a, "quake")) { MakeEarthquake(); }   /* debug */
        if (!strcmp(a, "help")) show_help();
        if (!strcmp(a, "savetest")) { save_city(win, "PROGDIR:cities/savetest.cty"); logmsg(status_msg); }   /* debug */
        if (!strncmp(a, "hover=", 6)) tool_hover = atoi(a + 6);   /* debug */
        if (!strncmp(a, "chalk", 5)) {                 /* debug: a note on the map */
            int bx = (view_x + 8) << 4, by = (view_y + 6) << 4;
            editor.tool_state = chalkState;
            ToolDown(&editor, bx, by);
            for (int k = 1; k <= 40; k++) ToolDrag(&editor, bx + k * 5, by + (k % 10) * 6 + k);
            ToolUp(&editor, bx + 200, by + 60);
            if (!strcmp(a, "chalkerase")) { editor.tool_state = eraserState; ToolDown(&editor, bx + 100, by + 40); ToolUp(&editor, bx + 100, by + 40); }
            editor.tool_state = queryState;
        }   /* debug */
        start_pending = start_menu && argc <= 2;                  /* not for test runs (debug arguments) */
        if (!strncmp(a, "show=", 5)) debug_show = 1;
        if (!strcmp(a, "show=about")) { show_about(); dbg_about = 1; }
        if (!strcmp(a, "newcity")) do_command(CMD_NEW, win, NULL);
        if (!strncmp(a, "coast=", 6)) { coast_freeze = atoi(a + 6); world_anim = 1; }   /* debug: show one surf step (with det) */
        if (!strcmp(a, "det")) { dbg_det = 1; DoAnimation = 0; world_anim = coast_freeze >= 0; ambient_on = 0; }
        if (!strncmp(a, "dump=", 5)) strncpy(dbg_dump, a + 5, sizeof dbg_dump - 1);
        if (!strcmp(a, "show=eval")) open_info_panel(P_EVAL);              /* debug: open a panel at start */
        if (!strcmp(a, "show=graph")) open_info_panel(P_GRAPH);
        if (!strcmp(a, "show=sound")) open_info_panel(P_SOUND);
        if (!strcmp(a, "show=budget")) DoBudgetFromMenu();
        if (!strncmp(a, "at=", 3)) { int jx = 0, jy = 0; sscanf(a + 3, "%d,%d", &jx, &jy); view_x = jx; view_y = jy; clamp_view(); }   /* debug: scroll there, keep the speed */
        if (!strncmp(a, "speed=", 6)) do_command(CMD_PAUSE + atoi(a + 6), win, NULL);   /* debug: as the Speed menu */
        if (!strcmp(a, "show=map")) { open_info_panel(P_MAP); map_dirty = 1; }
        if (!strncmp(a, "notice=", 7)) { notice_show(atoi(a + 7)); notice_dirty = 1; }   /* debug */
    }
    int running = 1, tick = 0, frame = 0, title_tick = 0;
    sound_ui_ready = 1;
    static const int delay[4] = { 0, 12, 4, 1 };      /* video frames per simulation step */
    while (running) {
        struct IntuiMessage *m;
        while ((m = (struct IntuiMessage *)GetMsg(idcmp_port))) {
            ULONG cls = m->Class;
            UWORD code = m->Code;
            int mx = m->MouseX, my = m->MouseY;
            UWORD qual = m->Qualifier;
            APTR iaddr = m->IAddress;
            struct Window *mw = m->IDCMPWindow;
            int pi = panel_index(mw);                             /* which panel window, or -1 = the game window */
            ReplyMsg((struct Message *)m);
            if (dragging && mw != win) end_drag();                    /* a requester or window took the mouse while a road/rail/wire was being drawn */
            if (cls == IDCMP_MENUPICK) {
                while (code != MENUNULL && menu) {
                    struct MenuItem *it = ItemAddress(menu, code);
                    if (!it) break;
                    if (!do_command((int)(long)GTMENUITEM_USERDATA(it), win, it)) running = 0;
                    refresh_full = 1;                                    /* a requester may have covered the map while the city changed */
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
                else { full_redraw = 1; first_present = 1; toolbar_dirty = status_dirty = demand_dirty = 1; tb_label_only = 0; last_line1[0] = last_line2[0] = 0; render_frame(win); }
                EndRefresh(mw, TRUE);
                if (refresh_full && pi < 0) { refresh_full = 0; full_redraw = 1; dirty = 1; }
                continue;
            }
            if (cls == IDCMP_RAWKEY) {
                int up = (code & IECODE_UP_PREFIX) != 0;
                if (KeymapBase) {                                   /* typed characters: tool shortcuts (b r p t x z) and cheat words, like the original */
                    struct InputEvent ie;
                    char kb[8];
                    memset(&ie, 0, sizeof ie);
                    ie.ie_Class = IECLASS_RAWKEY; ie.ie_Code = code & ~IECODE_UP_PREFIX; ie.ie_Qualifier = qual;
                    ie.ie_EventAddress = (APTR *)iaddr;
                    if (MapRawKey(&ie, (STRPTR)kb, sizeof kb, NULL) == 1 && (unsigned char)kb[0] >= 27 && (unsigned char)kb[0] < 127) {
                        if (!(qual & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND))) {
                            if (up) doKeyUp(&editor, kb[0]); else doKeyDown(&editor, kb[0]);
                        }
                    }
                }
                if (up) continue;
                { static int n; if (n < 25) { char b[40]; sprintf(b, "rawkey 0x%02x", (unsigned)code); logmsg(b); n++; } }
                switch (code) {
                case 0x7a: view_y -= 2; break;                            /* mouse wheel */
                case 0x7b: view_y += 2; break;
                case 0x11: view_y--; break;                               /* W A S D scroll, too */
                case 0x21: view_y++; break;
                case 0x22: view_x++; break;
                case 0x20: view_x--; break;
                case 0x37: if (!(qual & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND))) { map_visible = !map_visible; map_dirty = 1; } break;   /* M */
                case 0x5f: show_help(); break;                                       /* Help key */
                case 0x45: if (pi >= 0) panels[pi].close(); else if (ask_quit(win)) running = 0; break;   /* Esc: closes the active panel window, else quits */
                case 0x44: case 0x40: if (pi >= 0) panels[pi].close(); break;          /* Return, Space */
                case 0x4c: view_y--; break;                               /* cursor keys scroll */
                case 0x4d: view_y++; break;
                case 0x4e: view_x++; break;
                case 0x4f: view_x--; break;
                case 0x0a: setSpeed(0); break;                           /* 0 = pause */
                case 0x01: setSpeed(1); break;
                case 0x02: setSpeed(2); break;
                case 0x03: setSpeed(3); break;
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
                } else if (pi == P_SOUND) {                        /* sliders: press, drag, release */
                    int kind = cls == IDCMP_MOUSEMOVE ? 1 : code == SELECTDOWN ? 0 : code == SELECTUP ? 2 : -1;
                    if (cls == IDCMP_MOUSEMOVE || (cls == IDCMP_MOUSEBUTTONS && kind >= 0))
                        if (sound_ui_mouse(ax, ay, kind)) sound_dirty = 1;
                } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN && pn->click) {
                    pn->click(ax, ay);
                    *pn->dirty = 1;
                }
            } else {
                /* mouse in the game window: window coordinates -> tile / world pixel */
                int inview = mx >= TB_W && my >= BAR_H;
                int wx = (view_x << 4) + (mx - TB_W), wy = (view_y << 4) + (my - BAR_H);
                mouse_x = mx; mouse_y = my;
                {   int th = tool_at(mx, my);
                    if (th != tool_hover) { tool_hover = th; if (!toolbar_dirty) tb_label_only = 1; toolbar_dirty = 1; }
                }
                if (inview) { hover_x = wx >> 4; hover_y = wy >> 4; } else hover_x = -1;
                if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                    if (mx < TB_W && my >= BAR_H) {
                        int i = tool_at(mx, my);
                        if (i >= 0) {
                            static const char *const tool_sound[18] = { "res", "com", "ind", "fire", "query", "police", "wire", "bulldozer", "rail",
                                                                         "road", "chalk", "eraser", "stadium", "park", "seaport", "coal", "nuclear", "airport" };
                            editor.tool_state = i; toolbar_dirty = 1; tb_label_only = 0;
                            if (UserSoundOn && speak_tools && editor.tool_state >= 0 && editor.tool_state < 18) sound_play(tool_sound[editor.tool_state]);   /* like the original palette */
                        }
                    } else if (inview) {
                        ToolDown(&editor, wx, wy);
                        if (editor.tool_state != queryState && editor.tool_state != chalkState && editor.tool_state != eraserState) tool_used = 1;
                        dragging = 1; drag_x = wx; drag_y = wy;
                    }
                } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP) {
                    if (dragging) { ToolUp(&editor, inview ? wx : drag_x, inview ? wy : drag_y); dragging = 0; }
                } else if (cls == IDCMP_MOUSEMOVE && dragging && !(qual & IEQUALIFIER_LEFTBUTTON)) {
                    end_drag();                                          /* the button is up but we never saw it (it went up over another window) */
                } else if (cls == IDCMP_MOUSEMOVE && dragging && inview) {
                    ToolDrag(&editor, wx, wy); drag_x = wx; drag_y = wy;
                }
            }
            dirty = 1;
        }
        if (mouse_x >= 0 && !panning && (++frame & 3) == 0) {   /* edge scrolling */
            int ox = view_x, oy = view_y;
            if ((mouse_x >= TB_W && mouse_x < TB_W + 8) || mouse_x < 3) view_x--;   /* left: the strip next to the palette or the very edge of the screen */
            if (mouse_x >= SCR_W - 8) view_x++;
            if (mouse_y < BAR_H + 8 && mouse_x >= TB_W) view_y--;       /* top: the status bar counts, too */
            if (mouse_y >= SCR_H - 8) view_y++;
            clamp_view();
            if (ox != view_x || oy != view_y) dirty = 1;
        }
        sound_poll();
        if (anim_hold > 0) anim_hold--;
        else if (world_anim && coast_seen) {                           /* surf: next step every 145 ms (only while shore is on the screen) */
            struct timeval tn; int st;
            gettimeofday(&tn, NULL);
            st = coast_freeze >= 0 ? coast_freeze : (int)((((unsigned long)tn.tv_sec & 0xFFFF) * 1000UL + (unsigned long)(tn.tv_usec / 1000)) / (anim_slow ? 380 : 190));   /* small enough for 32 bit longs and never negative (the old product overflowed on the Amiga) */
            if (st != coast_tick) { coast_tick = st; dirty = 1; }
        }
        { static int amb_frame; if ((++amb_frame & 31) == 0 || amb_frame == 1) ambient_update(); ambient_tick(); }
        guide_poll();
        if (ShakeNow > 0 && shake_frames == 0) { shake_frames = 50; shake_bx = view_x; shake_by = view_y; }
        if (shake_frames > 0) {
            shake_frames--;
            if (shake_frames == 0) { view_x = shake_bx; view_y = shake_by; ShakeNow = 0; }
            else { view_x = shake_bx + (rand() % 3) - 1; view_y = shake_by + (rand() % 3) - 1; }
            clamp_view(); dirty = 1;
        }
        if (debug_show && notice_visible && !dbg_about) notice_close();
        if (dbg_scroll > 0 && (frame & 3) == 0) {
            static struct timeval ts; static int steps;
            if (!steps) gettimeofday(&ts, NULL);
            steps++; view_x++; if (dbg_scroll & 1) view_y++; dbg_scroll--; clamp_view(); dirty = 1;
            if (!dbg_scroll) {                                  /* debug: how long did the scroll test take? */
                struct timeval te; char b[80]; gettimeofday(&te, NULL);
                sprintf(b, "scroll test: %d steps in %ld ms (%d loops)", steps, (long)((te.tv_sec - ts.tv_sec) * 1000 + (te.tv_usec - ts.tv_usec) / 1000), frame);
                logmsg(b);
            }
        }
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
            if (!TilesAnimated) { if (DoAnimation) animateTiles(); TilesAnimated = 1; }
            render_frame(win);
            dirty = 0;
        }
        if ((++title_tick & 7) == 0) {                                    /* the screen title bar shows the city's numbers */
            char t[120];
            sprintf(t, "Micropolis %s   %s   %s   %s %ld   %s", VERSION, status_date, status_funds, T("Pop"), (long)CityPop,
                    SimSpeed ? "" : T("(paused)"));
            set_screen_title(t);
        }
        sync_panels();
        if (menu_sync_pending) { menu_sync_pending = 0; menu_sync(); }
        if (start_pending && !dirty) {                                    /* the first picture is on the screen: ask what to start with */
            start_pending = 0;
            start_choice(win);
            full_redraw = 1; dirty = 1; refresh_full = 1;
        }
        if (dbg_quit_loops && title_tick > dbg_quit_loops) running = 0;
        WaitTOF();
        gettimeofday(&now_time, NULL);
        flagBlink = dbg_det ? -1 : (now_time.tv_usec < 500000) ? 1 : -1;
    }
    if (dbg_dump[0]) {                                      /* test support: the exact picture (fb, panel scratch, palette), see tools/test */
        FILE *df = fopen(dbg_dump, "wb");
        if (df) {
            UWORD hdr[6] = { 0x4d50, (UWORD)SCR_W, (UWORD)SCR_H, (UWORD)SCRATCH_STRIDE, (UWORD)MAX_H, 0 };
            fwrite(hdr, 2, 6, df);
            fwrite(fb, 1, (size_t)SCR_W * SCR_H, df);
            fwrite(scratch, 1, (size_t)SCRATCH_STRIDE * MAX_H, df);
            fwrite(palette, 1, 768, df);
            fclose(df);
        }
    }
    rc = 0;
out:
    sound_exit();
    logmsg("cleanup");
    for (int i = 0; i < NPANELS; i++) panel_close(i);
    if (rc == 0 && !test_mode) {                                  /* remember options, speed and window positions */
        for (int o = 0; o < N_OPT; o++) pref_opt[o] = opt_get(o);
        if (SimSpeed) pref_speed = (int)SimSpeed;                  /* a paused game does not start paused next time */
        prefs_save();
    }
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
    if (guide_handle) { CloseAmigaGuide(guide_handle); guide_handle = NULL; }
    if (AmigaGuideBase) CloseLibrary(AmigaGuideBase);
    if (KeymapBase) CloseLibrary(KeymapBase);
    l10n_exit();
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
