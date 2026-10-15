/* Native (macOS/Linux) build of the simulation core without any UI, to compare its results with the Amiga build.
 * usage: headless_native city.cty [months] [per-month-line]
 * Build: tools/native_test.sh */
#include "sim.h"
#include "ui.h"

int  ui_eval(const char *cmd) { return 0; }
void ui_invalidate(int what) {}
void ui_flush(void) {}
void ui_update_heads(void) {}
void app_log(const char *msg) {}

int main(int argc, char **argv)
{
    int months = argc > 2 ? atoi(argv[2]) : 24, every = argc > 3 ? atoi(argv[3]) : 1, i;
    printf("native: short=%d int=%d long=%d QUAD=%d map=%dx%d\n", (int)sizeof(short), (int)sizeof(int), (int)sizeof(long),
           (int)sizeof(QUAD), WORLD_X, WORLD_Y);
    sim = MakeNewSim();
    HomeDir = ""; ResourceDir = "res"; HostName = "native";
    sim_init();
    if (!LoadCity(argv[1])) { printf("LoadCity failed\n"); return 10; }
    printf("loaded: year %d funds %ld pop %ld\n", (int)CurrentYear(), (long)TotalFunds, (long)CityPop);
    SimSpeed = 3;
    autoBudget = 1;                    /* no budget window here: let the budget run by itself, otherwise the simulation waits for it */
    for (i = 0; i < months * 16; i++) {
        sim_loop(1);
        if ((i + 1) % (16 * every) == 0)
            printf("month %3d: year %d funds %ld pop %ld score %d valves %d/%d/%d\n", (i + 1) / 16, (int)CurrentYear(),
                   (long)TotalFunds, (long)CityPop, (int)CityScore, (int)RValve, (int)CValve, (int)IValve);
    }
    return 0;
}
