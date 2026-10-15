/* Glue between the (Tcl/Tk/X11-free) simulation core in src/core and the Amiga frontend.
 * Upstream drives its UI by calling Eval("UIxxx args") on a Tcl interpreter; Eval() here is the
 * single entry point for that, so the frontend can handle those notifications natively.
 * Everything else below replaces functions that lived in upstream w_tk.c / w_x.c / w_keys.c. */
#include <stdlib.h>
#include "sim.h"
#include "ui.h"

int tkMustExit = 0;
int UpdateDelayed = 0;

Sim *MakeNewSim(void)
{
    Sim *s = (Sim *)calloc(1, sizeof(Sim));
    return s;
}
void EventuallyRedrawGraph(SimGraph *g) {}

/* --- UI notifications (strings from upstream's Tcl bridge) --- */
int Eval(char *cmd)
{
    return ui_eval(cmd);
}

/* --- redraw / update hooks: the frontend decides what to do --- */
void InvalidateMaps(void)    { sim_skip = 0; ui_invalidate(UI_MAPS); }
void InvalidateEditors(void) { sim_skip = 0; ui_invalidate(UI_EDITORS); }
void EventuallyRedrawView(SimView *v) { ui_invalidate(UI_EDITORS); }
void CancelRedrawView(SimView *v) {}
void RedrawMaps(void)        { UpdateDelayed = 0; sim_skip = 0; sim_update(); }
void Kick(void)              { UpdateDelayed = 1; }
void UpdateFlush(void)       { ui_flush(); }
int  DoUpdateEditor(SimView *v) { return 0; }
int  DoUpdateMap(SimView *v)    { return 0; }
void DoTimeoutListen(void)   {}
void DoStopMicropolis(void)  {}
void StartMicropolisTimer(void) {}
void StopMicropolisTimer(void)  {}
void setUpMapProcs(void)     {}

void StopEarthquake(void)    { ShakeNow = 0; }
void DoEarthQuake(void)
{
    MakeSound("city", "Explosion-Low");
    Eval("UIEarthQuake");
    ShakeNow++;
}

/* --- chalk/ink overlay: notes drawn on the map (as in the original; kept in world pixel coordinates) --- */
#define POINT_BATCH 32
static Ink *OldInk;

Ink *NewInk(void)
{
    Ink *ink;
    if (OldInk) { ink = OldInk; OldInk = OldInk->next; }
    else {
        ink = (Ink *)malloc(sizeof(Ink));
        if (!ink) return NULL;
        ink->maxlength = POINT_BATCH;
        ink->points = (XPoint *)malloc(POINT_BATCH * sizeof(XPoint));
        if (!ink->points) { free(ink); return NULL; }
    }
    ink->length = 0;
    ink->color = COLOR_WHITE;
    ink->next = NULL;
    ink->left = ink->right = ink->top = ink->bottom = ink->last_x = ink->last_y = -1;
    return ink;
}

void FreeInk(Ink *ink) { ink->next = OldInk; OldInk = ink; }

void StartInk(Ink *ink, int x, int y)
{
    ink->length = 1;
    ink->left = ink->right = ink->last_x = ink->points[0].x = x;
    ink->top = ink->bottom = ink->last_y = ink->points[0].y = y;
}

void AddInk(Ink *ink, int x, int y)       /* points[0] is absolute, the others are steps */
{
    int dx = x - ink->last_x, dy = y - ink->last_y;
    if (!dx && !dy) return;
    if (ink->length >= ink->maxlength) {
        XPoint *np = (XPoint *)realloc((void *)ink->points, (ink->maxlength + POINT_BATCH) * sizeof(XPoint));
        if (!np) return;
        ink->points = np; ink->maxlength += POINT_BATCH;
    }
    ink->points[ink->length].x = dx;
    ink->points[ink->length].y = dy;
    ink->length++;
    if (x < ink->left) ink->left = x;
    if (x > ink->right) ink->right = x;
    if (y < ink->top) ink->top = y;
    if (y > ink->bottom) ink->bottom = y;
    ink->last_x = x; ink->last_y = y;
}

void EraseOverlay(void)
{
    while (sim && sim->overlay) { Ink *ink = sim->overlay; sim->overlay = ink->next; FreeInk(ink); }
}
void ViewToPixelCoords(SimView *view, int x, int y, int *ox, int *oy) { *ox = x; *oy = y; }
