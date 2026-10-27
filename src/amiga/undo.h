#ifndef UNDO_H
#define UNDO_H
/* Undo / redo of building actions (up to 5 steps). The map is copied when a tool stroke begins; when the mouse button goes up
 * the tiles that changed are kept (with the money the stroke cost). Undo puts back only the tiles that still show what the stroke
 * made, so what the simulation did meanwhile (a zone that grew, ...) is not touched. */
#define UNDO_MAX 5
void undo_begin(void);                 /* a tool stroke starts (not for the query tool and the chalk) */
void undo_commit(void);                /* the stroke is over: keep what it changed */
int  undo_undo(void);                  /* returns the number of tiles put back, -1 when there is nothing to undo */
int  undo_redo(void);                  /* same for redo */
void undo_clear(void);                 /* a new city was loaded or made */
#endif
