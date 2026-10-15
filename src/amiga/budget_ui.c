/* Budget panel: modal overlay drawn into the frame buffer. The simulation core pauses itself and
 * sends "UIShowBudgetAndWait"; the numbers come from its "UISetBudget*" notifications, the
 * settings (tax rate, road/fire/police funding) are the core's own globals. Mirrors upstream
 * res/wbudget.tcl + the Budget* procs in res/micropolis.tcl. */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdio.h>
#include <string.h>
#include "l10n.h"
#include "gfx.h"
#include "budget_ui.h"

extern float roadPercent, policePercent, firePercent;
extern QUAD roadMaxValue, policeMaxValue, fireMaxValue;
void UpdateFundEffects(void);
void UpdateBudget(void);
void UpdateBudgetWindow(void);
void Resume(void);
void drawBudgetWindow(void);

int budget_visible;

/* panel geometry (frame buffer pixels) */
#define PX 120
#define PY 80
#define PW 400
#define PH 320
#define ROW_TAX    (PY + 150)
#define ROW_ROAD   (PY + 186)
#define ROW_FIRE   (PY + 222)
#define ROW_POLICE (PY + 258)
#define BTN_Y      (PY + 288)
#define MINUS_X    (PX + 150)
#define PLUS_X     (PX + 232)
#define STEP_W 28
#define STEP_H 20

static char flow[24], previous[24], current[24], collected[24];
static short old_tax;
static int old_road, old_fire, old_police;

static void get_braced(const char *s, int n, char *out, size_t max)
{
    out[0] = 0;
    while (n-- >= 0) {
        s = strchr(s, '{');
        if (!s) return;
        if (n < 0) {
            const char *e = strchr(s, '}');
            size_t len = e ? (size_t)(e - s - 1) : 0;
            if (len >= max) len = max - 1;
            memcpy(out, s + 1, len); out[len] = 0;
            return;
        }
        s = strchr(s, '}');
        if (!s) return;
    }
}

void budget_set_totals(const char *cmd)
{
    get_braced(cmd, 0, flow, sizeof flow);
    get_braced(cmd, 1, previous, sizeof previous);
    get_braced(cmd, 2, current, sizeof current);
    get_braced(cmd, 3, collected, sizeof collected);
}

void budget_set_values(const char *cmd) {}      /* percentages are read from the core's globals */

void budget_show(void)
{
    if (!budget_visible) {                      /* remember the values for "Cancel" (as upstream's BudgetReset) */
        old_tax = CityTax;
        old_road = (int)(roadPercent * 100.0 + 0.5);
        old_fire = (int)(firePercent * 100.0 + 0.5);
        old_police = (int)(policePercent * 100.0 + 0.5);
    }
    budget_visible = 1;
    UpdateBudget();
    UpdateBudgetWindow();
}

static void rect(UBYTE *fb, int stride, int x, int y, int w, int h, UBYTE c)
{
    for (int j = 0; j < h; j++) memset(fb + (y + j) * stride + x, c, w);
}

static void stepper(UBYTE *fb, int stride, int row)
{
    rect(fb, stride, MINUS_X, row, STEP_W, STEP_H, PEN_BTN);
    rect(fb, stride, PLUS_X, row, STEP_W, STEP_H, PEN_BTN);
}

void budget_draw(UBYTE *fb, int stride)
{
    if (!budget_visible) return;
    rect(fb, stride, PX - 2, PY - 2, PW + 4, PH + 4, PEN_TEXT);
    rect(fb, stride, PX, PY, PW, PH, PEN_BAR);
    stepper(fb, stride, ROW_TAX - 14);
    stepper(fb, stride, ROW_ROAD - 14);
    stepper(fb, stride, ROW_FIRE - 14);
    stepper(fb, stride, ROW_POLICE - 14);
    rect(fb, stride, PX + 40, BTN_Y, 140, 24, PEN_SEL);
    rect(fb, stride, PX + 220, BTN_Y, 140, 24, PEN_BTN);
}

#define put(rp, x, y, s) gfx_print((x), (y), (s))
#define SetAPen(rp, c) gfx_fg(c)
#define SetBPen(rp, c) gfx_bg(c)
static UBYTE cur_fg = PEN_TEXT, cur_bg = PEN_BAR;
static void gfx_fg(UBYTE c) { cur_fg = c; gfx_colors(cur_fg, cur_bg); }
static void gfx_bg(UBYTE c) { cur_bg = c; gfx_colors(cur_fg, cur_bg); }

void budget_text(void)
{
    void *rp = 0; (void)rp;
    char b[60];
    if (!budget_visible) return;
    SetAPen(rp, PEN_TEXT);
    SetBPen(rp, PEN_BAR);
    gfx_print_c(PX + 200, PY + 22, T("B U D G E T"));
    put(rp, PX + 16, PY + 52, T("Taxes collected")); gfx_print_r(PX + 16 + 28 * 8, PY + 52, collected);
    put(rp, PX + 16, PY + 72, T("Cash flow")); gfx_print_r(PX + 16 + 28 * 8, PY + 72, flow);
    put(rp, PX + 16, PY + 92, T("Previous funds")); gfx_print_r(PX + 16 + 28 * 8, PY + 92, previous);
    put(rp, PX + 16, PY + 112, T("Current funds")); gfx_print_r(PX + 16 + 28 * 8, PY + 112, current);

    SetBPen(rp, PEN_BTN);                            /* the +/- labels sit on the buttons */
    put(rp, MINUS_X + 10, ROW_TAX, "-");  put(rp, PLUS_X + 10, ROW_TAX, "+");
    SetBPen(rp, PEN_BAR);
    put(rp, PX + 16, ROW_TAX, T("Tax rate"));
    sprintf(b, "%3d%%", (int)CityTax);  put(rp, PX + 188, ROW_TAX, b);

    SetBPen(rp, PEN_BTN);
    put(rp, MINUS_X + 10, ROW_ROAD, "-");   put(rp, PLUS_X + 10, ROW_ROAD, "+");
    put(rp, MINUS_X + 10, ROW_FIRE, "-");   put(rp, PLUS_X + 10, ROW_FIRE, "+");
    put(rp, MINUS_X + 10, ROW_POLICE, "-"); put(rp, PLUS_X + 10, ROW_POLICE, "+");
    SetBPen(rp, PEN_BAR);
    put(rp, PX + 16, ROW_ROAD, T("Road fund"));
    sprintf(b, "%3d%%", (int)(roadPercent * 100.0 + 0.5));   put(rp, PX + 188, ROW_ROAD, b);
    put(rp, PX + 16, ROW_FIRE, T("Fire fund"));
    sprintf(b, "%3d%%", (int)(firePercent * 100.0 + 0.5));   put(rp, PX + 188, ROW_FIRE, b);
    put(rp, PX + 16, ROW_POLICE, T("Police fund"));
    sprintf(b, "%3d%%", (int)(policePercent * 100.0 + 0.5)); put(rp, PX + 188, ROW_POLICE, b);

    SetBPen(rp, PEN_SEL);   gfx_print_c(PX + 110, BTN_Y + 16, T("Continue"));
    SetBPen(rp, PEN_BTN);   gfx_print_c(PX + 290, BTN_Y + 16, T("Cancel"));
}

static void set_tax(int t)
{
    if (t < 0) t = 0;
    if (t > 20) t = 20;
    CityTax = t;
    drawBudgetWindow();
    Kick();
}

static void set_road(int p)
{
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    roadPercent = (float)p / 100.0f;
    RoadSpend = (roadMaxValue * p) / 100;
    UpdateFundEffects(); Kick();
}

static void set_fire(int p)
{
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    firePercent = (float)p / 100.0f;
    FireSpend = (fireMaxValue * p) / 100;
    UpdateFundEffects(); Kick();
}

static void set_police(int p)
{
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    policePercent = (float)p / 100.0f;
    PoliceSpend = (policeMaxValue * p) / 100;
    UpdateFundEffects(); Kick();
}

static void close_budget(int keep)
{
    if (!keep) {
        set_tax(old_tax); set_road(old_road); set_fire(old_fire); set_police(old_police);
        ClearMes(); SendMes(0);
    }
    budget_visible = 0;
    Resume();
}

void budget_key_close(void) { if (budget_visible) close_budget(1); }

int budget_click(int x, int y)
{
    int pct;
    if (!budget_visible) return 0;
    int minus_hit = x >= MINUS_X && x < MINUS_X + STEP_W;
    int plus_hit = x >= PLUS_X && x < PLUS_X + STEP_W;
    int d = minus_hit ? -1 : plus_hit ? 1 : 0;
#define IN_ROW(r) (y >= (r) - 14 && y < (r) - 14 + STEP_H)
    if (d && IN_ROW(ROW_TAX)) set_tax(CityTax + d);
    else if (d && IN_ROW(ROW_ROAD))   { pct = (int)(roadPercent * 100.0 + 0.5);   set_road(pct + 5 * d); }
    else if (d && IN_ROW(ROW_FIRE))   { pct = (int)(firePercent * 100.0 + 0.5);   set_fire(pct + 5 * d); }
    else if (d && IN_ROW(ROW_POLICE)) { pct = (int)(policePercent * 100.0 + 0.5); set_police(pct + 5 * d); }
    else if (y >= BTN_Y && y < BTN_Y + 24 && x >= PX + 40 && x < PX + 180) close_budget(1);
    else if (y >= BTN_Y && y < BTN_Y + 24 && x >= PX + 220 && x < PX + 360) close_budget(0);
    UpdateBudget(); UpdateBudgetWindow();
    return 1;
}
