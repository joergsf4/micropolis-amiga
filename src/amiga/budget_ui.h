#ifndef BUDGET_UI_H
#define BUDGET_UI_H
#include <exec/types.h>
extern int budget_visible;
#define BUDGET_X 118                          /* panel rectangle incl. border */
#define BUDGET_Y 78
#define BUDGET_W 404
#define BUDGET_H 324
void budget_show(void);                       /* core asked for the budget ("UIShowBudgetAndWait") */
void budget_set_totals(const char *cmd);      /* "UISetBudget {flow} {previous} {current} {collected} {tax}" */
void budget_set_values(const char *cmd);      /* "UISetBudgetValues ..." */
void budget_draw(UBYTE *fb, int stride);      /* panel graphics into the frame buffer */
void budget_text(void);                       /* panel text into the frame buffer (gfx_target must be set), after budget_draw */
int  budget_click(int x, int y);              /* mouse click; returns 1 when handled */
void budget_key_close(void);                  /* Esc / Return: continue */
#endif
