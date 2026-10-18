/* Native (macOS/Linux) build of the simulation core without any UI, to compare its results with the Amiga build.
 * usage: headless_native city.cty [months] [per-month-line]
 * Build: tools/native_test.sh */
#include "sim.h"
#include "ui.h"
extern float EMarket;

int  ui_eval(const char *cmd) { return 0; }
void ui_invalidate(int what) {}
void ui_flush(void) {}
void ui_update_heads(void) {}
void app_log(const char *msg) {}

#include "statehash.h"

static void adjacency(void)
{
    static const int px[12] = { -1, 0, 1, 2, 2, 2, 1, 0, -1, -2, -2, -2 }, py[12] = { -2, -2, -2, -1, 0, 1, 2, 2, 2, 1, 0, -1 };
    int kind, x, y, i, tot[3] = { 0, 0, 0 }, withroad[3] = { 0, 0, 0 };
    for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) {
        int raw = (unsigned short)Map[x][y], t = raw & LOMASK;
        if (!(raw & ZONEBIT)) continue;
        kind = (t >= RESBASE && t < COMBASE) ? 0 : (t >= COMBASE && t < INDBASE) ? 1 : (t >= INDBASE && t < 827) ? 2 : -1;
        if (kind < 0) continue;
        tot[kind]++;
        for (i = 0; i < 12; i++) {
            int xx = x + px[i], yy = y + py[i], tt;
            if (xx < 0 || yy < 0 || xx >= WORLD_X || yy >= WORLD_Y) continue;
            tt = (unsigned short)Map[xx][yy] & LOMASK;
            if (tt >= ROADBASE && tt < POWERBASE) { withroad[kind]++; break; }
        }
    }
    printf("zones with an adjacent road: res %d/%d  com %d/%d  ind %d/%d\n", withroad[0], tot[0], withroad[1], tot[1], withroad[2], tot[2]);
}

static void stats(void)
{   printf("pops res %ld com %ld ind %ld | hist res %d com %d ind %d | tax %d lvl %d emarket %ld | resCap %d comCap %d indCap %d | valves %d %d %d\n",
           (long)ResPop, (long)ComPop, (long)IndPop, (int)ResHis[1], (int)ComHis[1], (int)IndHis[1], (int)CityTax, (int)GameLevel, (long)(EMarket * 10),
           (int)ResCap, (int)ComCap, (int)IndCap, (int)RValve, (int)CValve, (int)IValve);
   int roads = 0, lt = 0, ht = 0, res = 0, com = 0, ind = 0, powered = 0, nzone = 0, rails = 0, wires = 0, tmax = 0, tnz = 0, x, y;
        for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) {
            int t = (unsigned short)Map[x][y] & LOMASK, raw = (unsigned short)Map[x][y];
            if (t >= ROADBASE && t < POWERBASE) { roads++; if (t >= HTRFBASE) ht++; else if (t >= LTRFBASE) lt++; }
            else if (t >= POWERBASE && t < RAILBASE) wires++;
            else if (t >= RAILBASE && t < RESBASE) rails++;
            if (raw & ZONEBIT) { nzone++; if (raw & PWRBIT) powered++; if (t >= RESBASE && t < COMBASE) res++; else if (t >= COMBASE && t < INDBASE) com++; else if (t >= INDBASE) ind++; }
        }
        { long sx = 0, sy = 0, n = 0; for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) { int t = (unsigned short)Map[x][y] & LOMASK; if (t >= ROADBASE && t < POWERBASE) { sx += x; sy += y; n++; } } if (n) printf("road centroid %ld,%ld\n", sx / n, sy / n); }
    for (x = 0; x < WORLD_X / 2; x++) for (y = 0; y < WORLD_Y / 2; y++) { if (TrfDensity[x][y] > tmax) tmax = TrfDensity[x][y]; if (TrfDensity[x][y]) tnz++; }
        printf("roads %d (light %d heavy %d) wires %d rails %d | zones %d (powered %d) res %d com %d ind %d | TrfDensity max %d, cells>0 %d\n",
               roads, lt, ht, wires, rails, nzone, powered, res, com, ind, tmax, tnz);
    }

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
    printf("at load: "); stats();
    if (argc > 3 && !strcmp(argv[3], "hash")) {        /* deterministic run: state hashes after every month */
        SeedRand(777);
        state_report(0);
        for (i = 0; i < months * 16; i++) { sim_loop(1); if ((i + 1) % 16 == 0) state_report((i + 1) / 16); }
        return 0;
    }
    autoBudget = 1;                    /* no budget window here: let the budget run by itself, otherwise the simulation waits for it */
    for (i = 0; i < months * 16; i++) {
        sim_loop(1);
        if ((i + 1) % (16 * every) == 0)
            printf("month %3d: year %d funds %ld pop %ld score %d valves %d/%d/%d\n", (i + 1) / 16, (int)CurrentYear(),
                   (long)TotalFunds, (long)CityPop, (int)CityScore, (int)RValve, (int)CValve, (int)IValve), stats();
    }
    stats(); adjacency();
    { extern long HopCount, TripShips, TripShipsDocked, TripPlanes, TripPlanesLanded; printf("traffic: %ld trips over ports/airports; ships %ld sent, %ld docked; planes %ld sent, %ld landed\n", HopCount, TripShips, TripShipsDocked, TripPlanes, TripPlanesLanded); }
    return 0;
}
