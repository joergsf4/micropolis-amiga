/* State hashes of the simulation, to compare the Amiga build with the native build step by step.
 * Included by tools/native/headless_native.c and src/amiga/headless.c (same output format; values are hashed, not bytes, so
 * the byte order of the machine does not matter). */
static unsigned long sh_h;
static void sh_add(long v) { sh_h = (sh_h ^ (unsigned long)(v & 0xffffffffUL)) * 16777619UL & 0xffffffffUL; }
#define SH_BEGIN() (sh_h = 2166136261UL)

static void state_report(int month)
{
    int x, y, i;
    printf("S%03d", month);
#define SH_2D_PTR(name, arr, w, h) SH_BEGIN(); for (x = 0; x < (w); x++) for (y = 0; y < (h); y++) sh_add(arr[x][y]); printf(" %s=%08lx", name, sh_h);
#define SH_2D_ARR(name, arr, w, h) SH_BEGIN(); for (x = 0; x < (w); x++) for (y = 0; y < (h); y++) sh_add(arr[x][y]); printf(" %s=%08lx", name, sh_h);
    SH_2D_PTR("map", Map, WORLD_X, WORLD_Y)
    SH_BEGIN(); for (i = 0; i < PWRMAPSIZE; i++) sh_add(PowerMap[i]); printf(" power=%08lx", sh_h);
    SH_2D_PTR("pop", PopDensity, HWLDX, HWLDY)
    SH_2D_PTR("trf", TrfDensity, HWLDX, HWLDY)
    SH_2D_PTR("poll", PollutionMem, HWLDX, HWLDY)
    SH_2D_PTR("lv", LandValueMem, HWLDX, HWLDY)
    SH_2D_PTR("crime", CrimeMem, HWLDX, HWLDY)
    SH_2D_PTR("terr", TerrainMem, QWX, QWY)
    SH_2D_ARR("rateog", RateOGMem, SmX, SmY)
    SH_2D_ARR("firest", FireStMap, SmX, SmY)
    SH_2D_ARR("police", PoliceMap, SmX, SmY)
    SH_2D_ARR("poleff", PoliceMapEffect, SmX, SmY)
    SH_2D_ARR("comrate", ComRate, SmX, SmY)
    SH_2D_ARR("firerate", FireRate, SmX, SmY)
    SH_BEGIN(); for (i = 0; i < 240; i++) { sh_add(ResHis[i]); sh_add(ComHis[i]); sh_add(IndHis[i]); sh_add(MoneyHis[i]); sh_add(CrimeHis[i]); sh_add(PollutionHis[i]); } printf(" hist=%08lx", sh_h);
    SH_BEGIN(); for (i = 0; i < 120; i++) sh_add(MiscHis[i]); printf(" misc=%08lx", sh_h);
    printf("\n     funds %ld pop %ld score %d | valves %d %d %d | crimeAvg %d pollAvg %d lvAvg %d | taxfund %ld | roadEff %d fireEff %d policeEff %d | sprites",
           (long)TotalFunds, (long)CityPop, (int)CityScore, (int)RValve, (int)CValve, (int)IValve, (int)CrimeAverage, (int)PolluteAverage,
           (int)LVAverage, (long)TaxFund, (int)RoadEffect, (int)FireEffect, (int)PoliceEffect);
    { SimSprite *sp; int n = 0; for (sp = sim->sprite; sp; sp = sp->next) n++; printf(" %d", n); }
    printf("\n");
}
