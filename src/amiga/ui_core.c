/* Glue between the (Tcl/Tk/X11-free) simulation core in src/core and the Amiga frontend.
 * Upstream drives its UI by calling Eval("UIxxx args") on a Tcl interpreter; Eval() here is the
 * single entry point for that, so the frontend can handle those notifications natively.
 * Everything else below replaces functions that lived in upstream w_tk.c / w_x.c / w_keys.c. */
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
void ResetLastKeys(void)     { PunishCnt = 0; }
void setUpMapProcs(void)     {}

void StopEarthquake(void)    { ShakeNow = 0; }
void DoEarthQuake(void)
{
    MakeSound("city", "Explosion-Low");
    Eval("UIEarthQuake");
    ShakeNow++;
}

/* --- chalk/ink overlay: not supported --- */
Ink *NewInk(void) { return NULL; }
void FreeInk(Ink *ink) {}
void StartInk(Ink *ink, int x, int y) {}
void AddInk(Ink *ink, int x, int y) {}
void ViewToPixelCoords(SimView *view, int x, int y, int *ox, int *oy) { *ox = x; *oy = y; }
