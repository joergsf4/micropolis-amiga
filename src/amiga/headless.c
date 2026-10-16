/* Milestone 1: run the simulation core without any UI.
 * usage: headless <city.cty> [months]   (default 24 months) */
#include "sim.h"
#include "ui.h"
extern int SeedRand();
extern float EMarket;
#include "stack.h"
#include "mapview.h"

void app_log(const char *msg) {}
static int verbose = 0;
int  ui_eval(const char *cmd) { if (verbose) printf("  [UI] %s\n", cmd); return 0; }
void ui_invalidate(int what) {}
void ui_flush(void) {}
void ui_update_heads(void) {}

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


static int real_main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int months = argc > 2 ? atoi(argv[2]) : 24;
    int i;
    verbose = argc > 3;
    printf("sizeof: short=%d int=%d long=%d QUAD=%d  map=%dx%d\n", (int)sizeof(short), (int)sizeof(int),
           (int)sizeof(long), (int)sizeof(QUAD), WORLD_X, WORLD_Y);
    if (argc < 2) { printf("usage: headless city.cty [months]\n"); return 5; }
    if (argc > 3 && !strcmp(argv[3], "hash")) {            /* deterministic run: state hashes after every month */
        sim = MakeNewSim();
        HomeDir = ""; ResourceDir = "res"; HostName = "amiga";
        sim_init();
        if (!LoadCity(argv[1])) { printf("LoadCity failed\n"); return 10; }
        SimSpeed = 3; SeedRand(777);
        for (i = 0; i < months * 16; i++) { sim_loop(1); if ((i + 1) % 16 == 0) state_report((i + 1) / 16); }
        return 0;
    }
    if (argc > 3 && !strcmp(argv[3], "floattest")) {
        volatile float a = 7, b = 3, c = 1.3f, d = 600, e;
        volatile short s1 = 200, s2 = 40; volatile long l1 = 123456;
        e = a / b;              printf("7/3*1000 = %ld (2333)\n", (long)(e * 1000));
        e = (a - 1) * d;        printf("(7-1)*600 = %ld (3600)\n", (long)e);
        e = s1 / 3.7;           printf("200/3.7*100 = %ld (5405)\n", (long)(e * 100));
        e = l1 * 0.02;          printf("123456*.02 = %ld (2469)\n", (long)e);
        e = (float)s1 / s2;     printf("200/40 = %ld (5)\n", (long)e);
        printf("1.3 > 1.2: %d (1), 1.0 > 1.3: %d (0), 0.5 < 0: %d (0)\n", (int)(c > 1.2f), (int)(1.0f > c), (int)(0.5f < 0));
        e = -0.5f; { short r = 100; r += e * 3; printf("100 += -1.5 -> %d (99)\n", (int)r); }
        { float x = 3000; short r = 1500; r += x; printf("short 1500 += 3000.0 -> %d (4500)\n", (int)r); }
        { float Rr = 0.3f; short V = 0; Rr = ((Rr - 1) * 600) + 200; printf("Rratio %ld (-220)\n", (long)Rr); if (Rr < 0) if (V > -2000) V += Rr; printf("V %d (-220)\n", (int)V); }
        return 0;
    }

    sim = MakeNewSim();
    HomeDir = ""; ResourceDir = "res"; HostName = "amiga";   /* env_init() is Unix-specific */
    sim_init();
    if (!LoadCity(argv[1])) { printf("LoadCity failed: %s\n", argv[1]); return 10; }
    printf("loaded %s: year %d, funds %ld, pop %ld, class %d\n", argv[1], (int)CurrentYear(),
           (long)TotalFunds, (long)CityPop, (int)CityClass);
    SimSpeed = 3;
    if (argc > 3 && !strcmp(argv[3], "mapdump")) {        /* render the overview map in every mode to files */
        char name[32];
        if (!map_load()) { printf("no res/tilessm.bin\n"); return 10; }
        for (i = 0; i < 80; i++) sim_loop(1);              /* let the census fill the density maps */
        for (map_mode = 0; map_mode < MAP_MODES; map_mode++) {
            FILE *f;
            map_refresh();
            sprintf(name, "map%02d.raw", map_mode);
            f = fopen(name, "wb");
            if (f) { fwrite(map_buffer(), 1, MV_W * MV_H, f); fclose(f); }
        }
        return 0;
    }
    if (argc > 3 && !strcmp(argv[3], "budget")) {      /* budget panel experiment */
        extern float roadPercent, policePercent, firePercent;
        extern void UpdateBudget(void), UpdateBudgetWindow(void), drawBudgetWindow(void), Kick(void);
        verbose = 1;
        printf("start: road %d fire %d police %d tax %d\n", (int)(roadPercent * 100.0 + 0.5),
               (int)(firePercent * 100.0 + 0.5), (int)(policePercent * 100.0 + 0.5), (int)CityTax);
        for (i = 0; i < 3; i++) {
            CityTax++; drawBudgetWindow(); Kick(); UpdateBudget(); UpdateBudgetWindow();
            printf("after click %d: road %d fire %d police %d tax %d (raw %ld)\n", i, (int)(roadPercent * 100.0 + 0.5),
                   (int)(firePercent * 100.0 + 0.5), (int)(policePercent * 100.0 + 0.5), (int)CityTax,
                   (long)(roadPercent * 1000.0));
        }
        for (i = 0; i < 4; i++) {
            int p = 100 - 5 * i;
            roadPercent = (float)p / 100.0f; firePercent = (float)p / 100.0f; policePercent = (float)p / 100.0f;
            UpdateBudget(); UpdateBudgetWindow();
            printf("set %d: road %d fire %d police %d (raw %ld)\n", p, (int)(roadPercent * 100.0 + 0.5),
                   (int)(firePercent * 100.0 + 0.5), (int)(policePercent * 100.0 + 0.5), (long)(roadPercent * 1000.0));
        }
        return 0;
    }
    for (i = 0; i < months * 16; i++) {      /* about 16 sim frames per month */
        sim_loop(1);
        if ((i + 1) % (16 * 6) == 0) { stats();
            printf("  month %3d: year %d funds %ld pop %ld score %d res/com/ind valve %d/%d/%d\n",
                   (i + 1) / 16, (int)CurrentYear(), (long)TotalFunds, (long)CityPop,
                   (int)CityScore, (int)RValve, (int)CValve, (int)IValve); }
    }
    stats(); adjacency();
    return 0;
}

int main(int argc, char **argv)
{
    return run_with_stack(real_main, argc, argv, BIG_STACK);
}
