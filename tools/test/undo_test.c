/* Native test of the undo module: build, undo, redo and compare the map and the funds. */
#include "sim.h"
#include "ui.h"
#include <stdio.h>
#include "undo.h"
int  ui_eval(const char *cmd) { return 0; }
void ui_invalidate(int what) {}
void ui_flush(void) {}
void ui_update_heads(void) {}
void app_log(const char *msg) {}
extern int ConnecTile(short x, short y, short *TileAdrPtr, short Command);
static unsigned long sum(void) { unsigned long s = 0; int x, y; for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) s = s * 31 + (unsigned short)Map[x][y]; return s; }
int main(int argc, char **argv)
{
    int fail = 0, x0 = -1, y0 = -1, i, k, x, y, n;
    sim = MakeNewSim(); HomeDir = ""; ResourceDir = "res"; HostName = "native";
    sim_init();
    if (!LoadCity(argv[1])) return 10;
    for (y = 3; y < WORLD_Y - 3 && x0 < 0; y++) for (x = 3; x < WORLD_X - 20 && x0 < 0; x++) {
        int ok = 1; for (i = 0; i < 15; i++) if ((Map[x + i][y] & LOMASK) != DIRT) ok = 0;
        if (ok) { x0 = x; y0 = y; }
    }
    TotalFunds = 5000;
    { unsigned long m0 = sum(); long f0 = (long)TotalFunds;
      /* 1: a road of 15 tiles is one stroke */
      undo_begin();
      for (i = 0; i < 15; i++) ConnecTile(x0 + i, y0, &Map[x0 + i][y0], 2);
      undo_commit();
      unsigned long m1 = sum(); long f1 = (long)TotalFunds;
      printf("road: funds %ld -> %ld, map changed %d\n", f0, f1, m1 != m0);
      if (m1 == m0 || f1 >= f0) { puts("FAIL: the road was not built"); fail = 1; }
      if (undo_undo() < 0 || sum() != m0 || (long)TotalFunds != f0) { puts("FAIL: undo did not restore map and funds"); fail = 1; } else puts("undo ok");
      if (undo_redo() < 0 || sum() != m1 || (long)TotalFunds != f1) { puts("FAIL: redo did not restore the road"); fail = 1; } else puts("redo ok");
      /* 2: five more strokes, the first one falls out of the history */
      for (k = 0; k < 6; k++) { undo_begin(); ConnecTile(x0 + k, y0 + 1, &Map[x0 + k][y0 + 1], 2); undo_commit(); }
      n = 0; while (undo_undo() >= 0) n++;
      printf("history depth %d (max %d)\n", n, UNDO_MAX);
      if (n != UNDO_MAX) { puts("FAIL: wrong history depth"); fail = 1; }
      /* 3: a tile that changed since is left alone */
      undo_clear();
      undo_begin(); ConnecTile(x0, y0 + 2, &Map[x0][y0 + 2], 2); undo_commit();
      Map[x0][y0 + 2] = (short)(RUBBLE | BULLBIT);                 /* the simulation did something with it */
      undo_undo();
      if ((Map[x0][y0 + 2] & LOMASK) != RUBBLE) { puts("FAIL: undo overwrote a tile that changed since"); fail = 1; } else puts("changed tile left alone ok");
      /* 4: nothing built: no history entry */
      undo_clear(); undo_begin(); undo_commit();
      if (undo_undo() != -1) { puts("FAIL: an empty stroke made an entry"); fail = 1; } else puts("empty stroke ok");
    }
    puts(fail ? "UNDO TEST FAILED" : "UNDO TEST PASSED");
    return fail;
}
