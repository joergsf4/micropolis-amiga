#include "sim.h"
/* AMIGA: only the history/graph data logic of upstream w_graph.c (Tk widget removed) */
short NewGraph = 0;
short AllMax;
unsigned char *History10[HISTORIES];
unsigned char *History120[HISTORIES];
int HistoryInitialized = 0;
short Graph10Max, Graph120Max;

void
drawMonth(short *hist, unsigned char *s, float scale)
{
  register short val;
  register short x;

  for (x = 0; x < 120; x++) {
    val = hist[x] * scale;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    s[119 - x] = val;
  }
}


void
doAllGraphs(void)
{
  float scaleValue;

  AllMax = 0;
  if (ResHisMax > AllMax) AllMax = ResHisMax;
  if (ComHisMax > AllMax) AllMax = ComHisMax;
  if (IndHisMax > AllMax) AllMax = IndHisMax;
  if (AllMax <= 128) AllMax = 0;

  if (AllMax) {
    scaleValue = 128.0 / AllMax;
  } else {
    scaleValue = 1.0;
  }

  // scaleValue = 0.5; // XXX

  drawMonth(ResHis, History10[RES_HIST], scaleValue);
  drawMonth(ComHis, History10[COM_HIST], scaleValue);
  drawMonth(IndHis, History10[IND_HIST], scaleValue);
  drawMonth(MoneyHis, History10[MONEY_HIST], 1.0);
  drawMonth(CrimeHis, History10[CRIME_HIST], 1.0);
  drawMonth(PollutionHis, History10[POLLUTION_HIST], 1.0);

  AllMax = 0;
  if (Res2HisMax > AllMax) AllMax = Res2HisMax;
  if (Com2HisMax > AllMax) AllMax = Com2HisMax;
  if (Ind2HisMax > AllMax) AllMax = Ind2HisMax;
  if (AllMax <= 128) AllMax = 0;

  if (AllMax) {
    scaleValue = 128.0 / AllMax;
  } else {
    scaleValue = 1.0;
  }

  // scaleValue = 0.5; // XXX

  drawMonth(ResHis + 120, History120[RES_HIST], scaleValue);
  drawMonth(ComHis + 120, History120[COM_HIST], scaleValue);
  drawMonth(IndHis + 120, History120[IND_HIST], scaleValue);
  drawMonth(MoneyHis + 120, History120[MONEY_HIST], 1.0);
  drawMonth(CrimeHis + 120, History120[CRIME_HIST], 1.0);
  drawMonth(PollutionHis + 120, History120[POLLUTION_HIST], 1.0);
}


void
ChangeCensus(void)
{
  CensusChanged = 1;
}


void
graphDoer(void)
{
  SimGraph *graph;

  if (CensusChanged) {
    doAllGraphs();
    NewGraph = 1;
    CensusChanged = 0;
  }

  if (NewGraph) {
    for (graph = sim->graph; graph != NULL; graph = graph->next) {
      EventuallyRedrawGraph(graph);
    }
    NewGraph = 0;
  }
}


void
initGraphs(void)
{
  int i;
  SimGraph *graph;

  for (graph = sim->graph; graph != NULL; graph = graph->next) {
    graph->range = 10;
    graph->mask = ALL_HISTORIES;
  }

  if (!HistoryInitialized) {
    HistoryInitialized = 1;
    for (i = 0; i < HISTORIES; i++) {
      History10[i] = (unsigned char *)ckalloc(120);
      History120[i] = (unsigned char *)ckalloc(120);
    }
  }
}


InitGraphMax(void)
{
  register x;

  ResHisMax = 0;
  ComHisMax = 0;
  IndHisMax = 0;
  for (x = 118; x >= 0; x--) {
    if (ResHis[x] > ResHisMax) ResHisMax = ResHis[x];
    if (ComHis[x] > ComHisMax) ComHisMax = ComHis[x];
    if (IndHis[x] > IndHisMax) IndHisMax = IndHis[x];
    if (ResHis[x] < 0) ResHis[x] = 0;
    if (ComHis[x] < 0) ComHis[x] = 0;
    if (IndHis[x] < 0) IndHis[x] = 0;
  }
  Graph10Max = ResHisMax;
  if (ComHisMax > Graph10Max) Graph10Max = ComHisMax;
  if (IndHisMax > Graph10Max) Graph10Max = IndHisMax;

  Res2HisMax = 0;
  Com2HisMax = 0;
  Ind2HisMax = 0;
  for (x = 238; x >= 120; x--) {
    if (ResHis[x] > Res2HisMax) Res2HisMax = ResHis[x];
    if (ComHis[x] > Com2HisMax) Com2HisMax = ComHis[x];
    if (IndHis[x] > Ind2HisMax) Ind2HisMax = IndHis[x];
    if (ResHis[x] < 0) ResHis[x] = 0;
    if (ComHis[x] < 0) ComHis[x] = 0;
    if (IndHis[x] < 0) IndHis[x] = 0;
  }
  Graph120Max = Res2HisMax;
  if (Com2HisMax > Graph120Max) Graph120Max = Com2HisMax;
  if (Ind2HisMax > Graph120Max) Graph120Max = Ind2HisMax;
}

