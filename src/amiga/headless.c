/* Milestone 1: run the simulation core without any UI.
 * usage: headless <city.cty> [months]   (default 24 months) */
#include "sim.h"
#include "ui.h"
#include "stack.h"

static int verbose = 0;
int  ui_eval(const char *cmd) { if (verbose) printf("  [UI] %s\n", cmd); return 0; }
void ui_invalidate(int what) {}
void ui_flush(void) {}
void ui_update_heads(void) {}

static int real_main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int months = argc > 2 ? atoi(argv[2]) : 24;
    int i;
    verbose = argc > 3;
    printf("sizeof: short=%d int=%d long=%d QUAD=%d  map=%dx%d\n", (int)sizeof(short), (int)sizeof(int),
           (int)sizeof(long), (int)sizeof(QUAD), WORLD_X, WORLD_Y);
    if (argc < 2) { printf("usage: headless city.cty [months]\n"); return 5; }

    sim = MakeNewSim();
    HomeDir = ""; ResourceDir = "res"; HostName = "amiga";   /* env_init() is Unix-specific */
    sim_init();
    if (!LoadCity(argv[1])) { printf("LoadCity failed: %s\n", argv[1]); return 10; }
    printf("loaded %s: year %d, funds %ld, pop %ld, class %d\n", argv[1], (int)CurrentYear(),
           (long)TotalFunds, (long)CityPop, (int)CityClass);
    SimSpeed = 3;
    for (i = 0; i < months * 16; i++) {      /* about 16 sim frames per month */
        sim_loop(1);
        if ((i + 1) % (16 * 6) == 0)
            printf("  month %3d: year %d funds %ld pop %ld score %d res/com/ind valve %d/%d/%d\n",
                   (i + 1) / 16, (int)CurrentYear(), (long)TotalFunds, (long)CityPop,
                   (int)CityScore, (int)RValve, (int)CValve, (int)IValve);
    }
    return 0;
}

int main(int argc, char **argv)
{
    return run_with_stack(real_main, argc, argv, BIG_STACK);
}
