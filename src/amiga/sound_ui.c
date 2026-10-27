/* Sound window (Options > Sound options...): the three switches that used to be menu items and a volume slider
 * for the sound effects and one for the ambient sounds. */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdio.h>
#include <string.h>
#include "l10n.h"
#include "gfx.h"
#include "sound.h"
#include "sound_ui.h"

int sound_visible;
int ambient_on = 1;
int speak_tools;

#define BOX 16                                   /* check box size */
#define TRACK_X 16
#define TRACK_W (SND_W - 32 - 56)                /* the percentage is printed right of the track */
#define KNOB_W 12
#define KNOB_H 20
#define ROW_SOUND  36
#define ROW_AMB    62
#define ROW_SPEAK  88
#define ROW_OUT    118                           /* the button that chooses the output: system sound or AHI */
#define ROW_FX     166                           /* slider rows: label above, track below */
#define ROW_AMBVOL 216
#define BTN_Y      (SND_H - 34)

static int drag = -1;                            /* slider being dragged: 0 effects, 1 ambient */

static void rect(UBYTE *fb, int stride, int x, int y, int w, int h, UBYTE c)
{
    for (int j = 0; j < h; j++) memset(fb + (y + j) * stride + x, c, w);
}

static void checkbox(UBYTE *fb, int stride, int row, int on, const char *label)
{
    rect(fb, stride, SND_X + 16, SND_Y + row - 12, BOX, BOX, PEN_TEXT);
    rect(fb, stride, SND_X + 18, SND_Y + row - 10, BOX - 4, BOX - 4, PEN_BTN);
    if (on) rect(fb, stride, SND_X + 21, SND_Y + row - 7, BOX - 10, BOX - 10, PEN_SEL);
    gfx_colors(PEN_TEXT, PEN_BAR);
    gfx_print(SND_X + 44, SND_Y + row, label);
}

static void slider(UBYTE *fb, int stride, int row, int value, const char *label)
{
    char b[16];
    int kx = TRACK_X + (TRACK_W - KNOB_W) * value / 100;
    gfx_colors(PEN_TEXT, PEN_BAR);
    gfx_print(SND_X + 16, SND_Y + row, label);
    rect(fb, stride, SND_X + TRACK_X, SND_Y + row + 16, TRACK_W, 6, PEN_TEXT);               /* groove */
    rect(fb, stride, SND_X + TRACK_X + 1, SND_Y + row + 17, TRACK_W - 2, 4, PEN_BTN);
    rect(fb, stride, SND_X + TRACK_X + 1, SND_Y + row + 17, kx - TRACK_X + KNOB_W / 2, 4, PEN_SEL);   /* filled part */
    rect(fb, stride, SND_X + kx, SND_Y + row + 9, KNOB_W, KNOB_H, PEN_TEXT);                 /* knob */
    rect(fb, stride, SND_X + kx + 1, SND_Y + row + 10, KNOB_W - 2, KNOB_H - 2, PEN_GREY);
    sprintf(b, "%3d%%", value);
    gfx_colors(PEN_TEXT, PEN_BAR);
    gfx_print(SND_X + TRACK_X + TRACK_W + 12, SND_Y + row + 24, b);
}

void sound_ui_draw(UBYTE *fb, int stride)
{
    rect(fb, stride, SND_X, SND_Y, SND_W, SND_H, PEN_BAR);
    checkbox(fb, stride, ROW_SOUND, UserSoundOn, T("Sound"));
    checkbox(fb, stride, ROW_AMB, ambient_on, T("Ambient sounds"));
    checkbox(fb, stride, ROW_SPEAK, speak_tools, T("Speak tool names"));
    {   char lb[48];                                                       /* output button */
        rect(fb, stride, SND_X + 16, SND_Y + ROW_OUT - 14, SND_W - 32, 22, PEN_BTN);
        gfx_colors(PEN_TEXT, PEN_BTN);
        gfx_print_c(SND_X + SND_W / 2, SND_Y + ROW_OUT + 2, sound_output_label(lb));
        gfx_print(SND_X + 24, SND_Y + ROW_OUT + 2, "<");                    /* the button is a selector: < goes back, > goes on */
        gfx_print_r(SND_X + SND_W - 24, SND_Y + ROW_OUT + 2, ">");
    }
    slider(fb, stride, ROW_FX, sound_fx_vol, T("Effects volume"));
    slider(fb, stride, ROW_AMBVOL, sound_amb_vol, T("Ambient volume"));
    rect(fb, stride, SND_X + SND_W / 2 - 50, SND_Y + BTN_Y, 100, 24, PEN_SEL);
    gfx_colors(PEN_TEXT, PEN_SEL);
    gfx_print_c(SND_X + SND_W / 2, SND_Y + BTN_Y + 16, T("Close"));
}

static void set_from_x(int which, int x)
{
    int v = (x - TRACK_X - KNOB_W / 2) * 100 / (TRACK_W - KNOB_W);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (which == 0) sound_fx_vol = v; else sound_amb_vol = v;
}

int sound_ui_mouse(int x, int y, int kind)
{
    x -= SND_X; y -= SND_Y;
    if (kind == 2) { int was = drag >= 0; drag = -1; return was; }
    if (kind == 1) { if (drag < 0) return 0; set_from_x(drag, x); return 1; }
    if (y >= ROW_SOUND - 12 && y < ROW_SOUND + 6 && x >= 16 && x < SND_W - 16) { UserSoundOn = !UserSoundOn; return 1; }
    if (y >= ROW_AMB - 12 && y < ROW_AMB + 6 && x >= 16 && x < SND_W - 16) { ambient_on = !ambient_on; return 1; }
    if (y >= ROW_SPEAK - 12 && y < ROW_SPEAK + 6 && x >= 16 && x < SND_W - 16) { speak_tools = !speak_tools; return 1; }
    if (y >= ROW_OUT - 14 && y < ROW_OUT + 8 && x >= 16 && x < SND_W - 16) { sound_cycle_output(x < 72 ? -1 : 1); return 1; }
    if (y >= ROW_FX + 6 && y < ROW_FX + 34 && x >= TRACK_X && x < TRACK_X + TRACK_W) { drag = 0; set_from_x(0, x); return 1; }
    if (y >= ROW_AMBVOL + 6 && y < ROW_AMBVOL + 34 && x >= TRACK_X && x < TRACK_X + TRACK_W) { drag = 1; set_from_x(1, x); return 1; }
    if (y >= BTN_Y && y < BTN_Y + 24 && x >= SND_W / 2 - 50 && x < SND_W / 2 + 50) { sound_ui_close(); return 1; }
    return 0;
}

void sound_ui_close(void) { sound_visible = 0; drag = -1; }
