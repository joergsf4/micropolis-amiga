#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdlib.h>
#include <string.h>
#include "undo.h"

struct cell { unsigned char x, y; unsigned short oldv, newv; };
struct rec { int n; struct cell *c; long cost; };

static unsigned short base[WORLD_X][WORLD_Y];
static QUAD base_funds;
static int have_base;
static struct rec undo_s[UNDO_MAX], redo_s[UNDO_MAX];
static int n_undo, n_redo;

extern short NewPower;
void UpdateFunds(void);

static void free_rec(struct rec *r) { free(r->c); r->c = NULL; r->n = 0; }

static void push(struct rec *stack, int *n, struct rec r)       /* the newest on top; the oldest falls out */
{
    if (*n == UNDO_MAX) { free_rec(&stack[0]); memmove(&stack[0], &stack[1], sizeof(struct rec) * (UNDO_MAX - 1)); (*n)--; }
    stack[(*n)++] = r;
}

void undo_begin(void)
{
    int x, y;
    for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) base[x][y] = (unsigned short)Map[x][y];
    base_funds = TotalFunds;
    have_base = 1;
}

void undo_commit(void)
{
    struct rec r;
    int n = 0, x, y, i;
    if (!have_base) return;
    have_base = 0;
    for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) if ((unsigned short)Map[x][y] != base[x][y]) n++;
    if (!n) return;                                              /* nothing was built */
    r.c = (struct cell *)malloc(sizeof(struct cell) * (size_t)n);
    if (!r.c) return;
    r.n = 0;
    for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++)
        if ((unsigned short)Map[x][y] != base[x][y]) { struct cell *c = &r.c[r.n++]; c->x = (unsigned char)x; c->y = (unsigned char)y; c->oldv = base[x][y]; c->newv = (unsigned short)Map[x][y]; }
    r.cost = base_funds > TotalFunds ? (long)(base_funds - TotalFunds) : 0;    /* money that came in meanwhile (taxes) is not a refund */
    for (i = 0; i < n_redo; i++) free_rec(&redo_s[i]);
    n_redo = 0;                                                  /* a new action ends the redo history */
    push(undo_s, &n_undo, r);
}

static int apply(struct rec *r, int back)                        /* back = 1: undo (put the old tiles), 0: redo */
{
    int done = 0, i;
    for (i = 0; i < r->n; i++) {
        struct cell *c = &r->c[i];
        unsigned short from = back ? c->newv : c->oldv, to = back ? c->oldv : c->newv;
        if ((unsigned short)Map[c->x][c->y] == from) { Map[c->x][c->y] = (short)to; done++; }
    }
    if (back) TotalFunds += r->cost; else TotalFunds -= r->cost;
    UpdateFunds();
    NewPower = 1;                                                /* the power net may have changed: look again */
    Kick();
    return done;
}

int undo_undo(void)
{
    struct rec r;
    int done;
    if (!n_undo) return -1;
    r = undo_s[--n_undo];
    done = apply(&r, 1);
    push(redo_s, &n_redo, r);
    return done;
}

int undo_redo(void)
{
    struct rec r;
    int done;
    if (!n_redo) return -1;
    r = redo_s[--n_redo];
    done = apply(&r, 0);
    push(undo_s, &n_undo, r);
    return done;
}

void undo_clear(void)
{
    int i;
    for (i = 0; i < n_undo; i++) free_rec(&undo_s[i]);
    for (i = 0; i < n_redo; i++) free_rec(&redo_s[i]);
    n_undo = n_redo = 0;
    have_base = 0;
}
