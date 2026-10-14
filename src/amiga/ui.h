/* Frontend interface used by ui_core.c. Implemented by the headless test or the real UI. */
#ifndef UI_H
#define UI_H
#define PEN_BAR   32                   /* palette entries behind the game's own 15 colors */
#define PEN_TEXT  33
#define PEN_SEL   34
#define PEN_BTN   35
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
void DoBudgetFromMenu(void);
void ClearMes(void);
void SendMes(int mes);
void Kick(void);
void GenerateNewCity(void);
int  SaveCityAs(char *filename);
void MakeFire(void);
void MakeFlood(void);
void MakeTornado(void);
void MakeEarthquake(void);
void MakeMonster(void);
void MakeMeltdown(void);
int  ToolDown(SimView *view, int x, int y);
int  ToolUp(SimView *view, int x, int y);
int  ToolDrag(SimView *view, int x, int y);
void MakeSound(char *channel, char *id);
#endif
