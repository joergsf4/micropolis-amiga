/* Frontend interface used by ui_core.c. Implemented by the headless test or the real UI. */
#ifndef UI_H
#define UI_H
#define UI_MAPS    1
#define UI_EDITORS 2
int  ui_eval(const char *cmd);
void ui_invalidate(int what);
void ui_flush(void);
void ui_update_heads(void);

/* simulation core entry points that upstream headers don't prototype */
void sim_init(void);
void sim_update(void);
void sim_loop(int doSim);
int  LoadCity(char *filename);
int  CurrentYear(void);
void animateTiles(void);
int  ToolDown(SimView *view, int x, int y);
int  ToolUp(SimView *view, int x, int y);
int  ToolDrag(SimView *view, int x, int y);
void MakeSound(char *channel, char *id);
#endif
