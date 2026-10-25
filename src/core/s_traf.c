/* s_traf.c
 *
 * Micropolis, Unix Version.  This game was released for the Unix platform
 * in or about 1990 and has been modified for inclusion in the One Laptop
 * Per Child program.  Copyright (C) 1989 - 2007 Electronic Arts Inc.  If
 * you need assistance with this program, you may contact:
 *   http://wiki.laptop.org/go/Micropolis  or email  micropolis@laptop.org.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  You should have received a
 * copy of the GNU General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 * 
 *             ADDITIONAL TERMS per GNU GPL Section 7
 * 
 * No trademark or publicity rights are granted.  This license does NOT
 * give you any right, title or interest in the trademark SimCity or any
 * other Electronic Arts trademark.  You may not distribute any
 * modification of this program using the trademark SimCity or claim any
 * affliation or association with Electronic Arts Inc. or its employees.
 * 
 * Any propagation or conveyance of this program must include this
 * copyright notice and these terms.
 * 
 * If you convey this program (or any modifications of it) and assume
 * contractual liability for the program to recipients of it, you agree
 * to indemnify Electronic Arts for any liability that those contractual
 * assumptions impose on Electronic Arts.
 * 
 * You may not misrepresent the origins of this program; modified
 * versions of the program must be marked as such and not identified as
 * the original program.
 * 
 * This disclaimer supplements the one included in the General Public
 * License.  TO THE FULLEST EXTENT PERMISSIBLE UNDER APPLICABLE LAW, THIS
 * PROGRAM IS PROVIDED TO YOU "AS IS," WITH ALL FAULTS, WITHOUT WARRANTY
 * OF ANY KIND, AND YOUR USE IS AT YOUR SOLE RISK.  THE ENTIRE RISK OF
 * SATISFACTORY QUALITY AND PERFORMANCE RESIDES WITH YOU.  ELECTRONIC ARTS
 * DISCLAIMS ANY AND ALL EXPRESS, IMPLIED OR STATUTORY WARRANTIES,
 * INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY, SATISFACTORY QUALITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS, AND WARRANTIES (IF ANY) ARISING FROM A COURSE OF DEALING,
 * USAGE, OR TRADE PRACTICE.  ELECTRONIC ARTS DOES NOT WARRANT AGAINST
 * INTERFERENCE WITH YOUR ENJOYMENT OF THE PROGRAM; THAT THE PROGRAM WILL
 * MEET YOUR REQUIREMENTS; THAT OPERATION OF THE PROGRAM WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT THE PROGRAM WILL BE COMPATIBLE
 * WITH THIRD PARTY SOFTWARE OR THAT ANY ERRORS IN THE PROGRAM WILL BE
 * CORRECTED.  NO ORAL OR WRITTEN ADVICE PROVIDED BY ELECTRONIC ARTS OR
 * ANY AUTHORIZED REPRESENTATIVE SHALL CREATE A WARRANTY.  SOME
 * JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF OR LIMITATIONS ON IMPLIED
 * WARRANTIES OR THE LIMITATIONS ON THE APPLICABLE STATUTORY RIGHTS OF A
 * CONSUMER, SO SOME OR ALL OF THE ABOVE EXCLUSIONS AND LIMITATIONS MAY
 * NOT APPLY TO YOU.
 */
#include "sim.h"


/* Traffic Generation */


#define MAXDIS 30

short PosStackN, SMapXStack[MAXDIS+1], SMapYStack[MAXDIS+1];
short LDir;
short Zsource;
short TrafMaxX, TrafMaxY;

/* Traffic rules (experiment, see issue #37): jams slow the zones next to a busy road, a rail connection keeps trips off
 * the roads and attracts growth, a failed trip costs growth, and ports / airports link separated areas. */
#ifndef TRAFFIC_RULES_DEFAULT
#define TRAFFIC_RULES_DEFAULT 1
#endif
short TrafficRules = TRAFFIC_RULES_DEFAULT;
short ZoneJam;			/* traffic density at the road a zone starts from (0..240); set by MakeTraf */
short ZoneOnRail;		/* the zone has a rail tile next to it */
short ZoneRailBonus;		/* growth bonus of the last trip: up to 150, by the share of its steps that went on rails */
static short RailSteps, RoadSteps;	/* steps of the trip on rails / on roads and highways */
short ZoneTested;		/* MakeTraf ran for this zone in this pass */
short HubX[2][8], HubY[2][8], HubN[2];		/* ports (0) and airports (1) with power: zone centre tiles (of the last complete map scan) */
short NextHubX[2][8], NextHubY[2][8], NextHubN[2];	/* the scan in progress */
short HubRX[2][8], HubRY[2][8], NextHubRX[2][8], NextHubRY[2][8];	/* a road tile next to each hub: where a trip enters and leaves it */
#define HUBSTATS 16
#ifndef HUB_CAPACITY
#define HUB_CAPACITY 1000		/* trips per year one port or airport can carry: more are refused (the trip fails); an island of ten populated factories needs about 400 */
#endif
static struct { short kind, x, y, now, last, warned; } HubStat[HUBSTATS];

static short HubStatFor(short kind, short x, short y)		/* the counter of the hub at x, y (made on first use) */
{
  short i, free = -1;
  for (i = 0; i < HUBSTATS; i++) {
    if (HubStat[i].x == x && HubStat[i].y == y && HubStat[i].kind == kind + 1) return (i);
    if (free < 0 && !HubStat[i].kind) free = i;
  }
  if (free < 0) free = 0;
  HubStat[free].kind = kind + 1; HubStat[free].x = x; HubStat[free].y = y; HubStat[free].now = HubStat[free].last = 0; HubStat[free].warned = 0;
  return (free);
}

void HubYearRoll(void)						/* once a year (at tax time) */
{
  short i;
  for (i = 0; i < HUBSTATS; i++) { HubStat[i].last = HubStat[i].now; HubStat[i].now = 0; HubStat[i].warned = 0; }
}

static short HubSea[8];				/* water body of each port (0 = none), filled by HubWater */
static short HubSeaFor[8][2], HubSeaN;		/* the port list HubSea was computed for */
static short Hopped;
long TripShips, TripShipsDocked, TripPlanes, TripPlanesLanded;	/* statistics of the scheduled ships and planes */
long TripAll, TripOk, TripFail, TripNoRoad;		/* statistics of MakeTraf */
long HopCount;			/* trips that went on over a port or airport (statistics) */


/* comefrom: DoIndustrial DoCommercial DoResidential */
#define HUB_LOAD 50		/* density a hub trip adds to the roads it uses per tile (like a normal trip) */
static short HubTrip(short sx, short sy);
static void RailBonus(void)			/* a successful trip earns the rail bonus by the share of rail steps it took */
{
  if (RailSteps + RoadSteps > 0) ZoneRailBonus = RailSteps * 150 / (RailSteps + RoadSteps);
}
static short IsolatedFor(int Zt, short sx, short sy);
MakeTraf(int Zt)
{
  short xtem, ytem;

  xtem = SMapX;
  ytem = SMapY;
  Zsource = Zt;
  PosStackN = 0;
  Hopped = 0;
  ZoneJam = 0;
  ZoneOnRail = 0;
  ZoneRailBonus = 0;
  RailSteps = RoadSteps = 0;
  ZoneTested = 1;

#if 0
  if ((!Rand(2)) && FindPTele()) {
/* printf("Telecommute!\n"); */
    return (TRUE);
  }
#endif

  if (FindPRoad()) {		/* look for road on zone perimeter */
    if (TrafficRules) {
      short t = Map[SMapX][SMapY] & LOMASK;
      if (t >= RAILBASE && t <= LASTRAIL) ZoneOnRail = 1;
      else ZoneJam = TrfDensity[SMapX >>1][SMapY >>1];
    }
    short sx = SMapX, sy = SMapY;		/* the road the trip starts on */
    TripAll++;
    if (TryDrive()) {
      TripOk++;		/* attempt to drive somewhere */
      SetTrafMem();		/* if sucessful, inc trafdensity (rail tiles are not counted: only the road steps load the roads) */
      if (TrafficRules) RailBonus();
      SMapX = xtem;
      SMapY = ytem;
      return (TRUE);		/* traffic passed */
    }
    if (TrafficRules && IsolatedFor(Zt, sx, sy) && HubTrip(sx, sy)) {	/* nothing found by wandering: the way to a port or airport, and on from there */
      RailBonus();
      SMapX = xtem;
      SMapY = ytem;
      return (TRUE);
    }
    SMapX = xtem;
    SMapY = ytem;
    TripFail++;
    return (FALSE);		/* traffic failed */
  }
  else { TripNoRoad++; return (-1); }		/* no road found */
}


/* comefrom: MakeTraf */
SetTrafMem(void)
{
  register short x, z;

  for (x = PosStackN; x > 0; x--) {
    PullPos();
    if (TestBounds(SMapX, SMapY)) {
      z = Map[SMapX][SMapY] & LOMASK;
      if ((z >= ROADBASE) && (z < POWERBASE)) {
	SimSprite *sprite;

	/* check for rail */
	z = TrfDensity[SMapX >>1][SMapY >>1];
	z += 50;
	if ((z > 240) &&
	    (!Rand(5))) {
	  z = 240;
	  TrafMaxX = SMapX <<4;
	  TrafMaxY = SMapY <<4;
	  if (((sprite = GetSprite(COP)) != NULL) &&
	      (sprite->control == -1)) {
	    sprite->dest_x = TrafMaxX;
	    sprite->dest_y = TrafMaxY;
	  }
	}
	TrfDensity[SMapX >>1][SMapY >>1] = z;
      }
    }
  }
}


/* comefrom: TryGo */
PushPos(void)
{
  if (PosStackN >= MAXDIS) return;		/* highway steps are free, so a trip can be longer than the stack: the rest is not counted */
  PosStackN++;
  SMapXStack[PosStackN] = SMapX;
  SMapYStack[PosStackN] = SMapY;
}


/* comefrom: SetTrafMem */
PullPos(void)
{
  SMapX = SMapXStack[PosStackN];
  SMapY = SMapYStack[PosStackN];
  PosStackN--;
}


/* comefrom: DoSPZone MakeTraf */
FindPRoad(void)		/* look for road on edges of zone   */
{
  static short PerimX[12] = {-1, 0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2};
  static short PerimY[12] = {-2,-2,-2,-1, 0, 1, 2, 2, 2, 1, 0,-1};
  register short tx, ty, z;

  if (TrafficRules && ZoneTested) {		/* a rail connection comes first (only for the zones' trips) */
    for (z = 0; z < 12; z++) {
      tx = SMapX + PerimX[z];
      ty = SMapY + PerimY[z];
      if (TestBounds(tx, ty)) {
	short t = Map[tx][ty] & LOMASK;
	if (t >= RAILBASE && t <= LASTRAIL) { SMapX = tx; SMapY = ty; return (TRUE); }
      }
    }
  }

  for (z = 0; z < 12; z++) {
	  tx = SMapX + PerimX[z];
	  ty = SMapY + PerimY[z];
	  if (TestBounds(tx, ty)) {
		  if (RoadTest(Map[tx][ty]) && !IsHwy(Map[tx][ty])) {		/* zones do not open onto a highway */
			  SMapX = tx;
			  SMapY = ty;
			  return (TRUE);
		  }
	  }
  }
  return (FALSE);
}


FindPTele(void)		/* look for telecommunication on edges of zone */
{
  static short PerimX[12] = {-1, 0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2};
  static short PerimY[12] = {-2,-2,-2,-1, 0, 1, 2, 2, 2, 1, 0,-1};
  register short tx, ty, z, tile;

  for (z = 0; z < 12; z++) {
	  tx = SMapX + PerimX[z];
	  ty = SMapY + PerimY[z];
	  if (TestBounds(tx, ty)) {
	  	  tile = Map[tx][ty] & LOMASK;
		  if ((tile >= TELEBASE) && (tile <= TELELAST)) {
			  return (TRUE);
		  }
	  }
  }
  return (FALSE);
}


/* comefrom: MakeTraf */
TryDrive(void)
{
  short z, free = 0;

  LDir = 5;
  for (z = 0; z < MAXDIS; z++) {	/* Maximum distance to try */
    if (TryGo(z)) {			/* if it got a road */
      if (TrafficRules && IsHwy(Map[SMapX][SMapY]) && free < 200) {	/* a highway is fast: three of four of its tiles do not use up the distance */
	free++;
	if (free & 3) z--;
      }
      if (DriveDone())			/* if destination is reached */
	return (TRUE);			/* pass */
      if (TrafficRules && !Hopped) HubHop();	/* next to a port or airport: continue from another one */
    } else {
      if (PosStackN) {			/* deadend , backup */
	PosStackN--;
	z += 3;
      }
      else return (FALSE);		/* give up at start  */	
    }
  }
  return (FALSE);			/* gone maxdis */
}


/* comefrom: TryDrive */
TryGo(int z)
{
  short x, rdir, realdir;

#if 0
  rdir = Rand(3); /* XXX: Heaviest user of Rand */
#else
  rdir = Rand16() & 3;
#endif
  for (x = rdir; x < (rdir + 4); x++) {	/* for the 4 directions */
    realdir = x & 3;
    if (realdir == LDir) continue;	/* skip last direction */
    if (RoadTest(GetFromMap(realdir))) {
      MoveMapSim(realdir);
      { short st = Map[SMapX][SMapY] & LOMASK; if (st >= RAILBASE && st <= LASTRAIL) RailSteps++; else RoadSteps++; }
      LDir = (realdir + 2) & 3;
      if (z & 1)			/* save pos every other move */
	PushPos();
      return (TRUE);
    }
  }
  return (FALSE);
}


/* comefrom: TryGo DriveDone */
GetFromMap(int x)
{
  switch (x) {
  case 0:
    if (SMapY > 0)
      return (Map[SMapX][SMapY - 1] & LOMASK);
    return (FALSE);
  case 1:
    if (SMapX < (WORLD_X - 1))
      return (Map[SMapX + 1][SMapY] & LOMASK);
    return (FALSE);
  case 2:
    if (SMapY < (WORLD_Y - 1))
      return (Map[SMapX][SMapY + 1] & LOMASK);
    return (FALSE);
  case 3:
    if (SMapX > 0)
      return (Map[SMapX - 1][SMapY] & LOMASK);
    return (FALSE);
  default: 
    return (FALSE);
  }
}


/* comefrom: TryDrive */
DriveDone(void)
{
  static short TARGL[3] = {COMBASE, LHTHR, LHTHR};
  static short TARGH[3] = {NUCLEAR, PORT, COMBASE};	/* for destinations */
  register short x, z, l, h;

/* unwound -Don */
#if 0
  for (x = 0; x < 4; x++) {	/* R>C C>I I>R  */
    z = GetFromMap(x);
    if ((z >= TARGL[Zsource]) && (z <= TARGH[Zsource]))
      return (TRUE);
  }
#else
  l = TARGL[Zsource];
  h = TARGH[Zsource];

  if (SMapY > 0) {
    z = Map[SMapX][SMapY - 1] & LOMASK;
    if ((z >= l) && (z <= h))
      return (TRUE);
  }
  if (SMapX < (WORLD_X - 1)) {
    z = Map[SMapX + 1][SMapY] & LOMASK;
    if ((z >= l) && (z <= h))
      return (TRUE);
  }
  if (SMapY < (WORLD_Y - 1)) {
    z = Map[SMapX][SMapY + 1] & LOMASK;
    if ((z >= l) && (z <= h))
      return (TRUE);
  }
  if (SMapX > 0) {
    z = Map[SMapX - 1][SMapY] & LOMASK;
    if ((z >= l) && (z <= h))
      return (TRUE);
  }
#endif

  return (FALSE);
}


/* Ports and airports link separated areas. A hub only counts when a road touches it (the trips come in and leave over
 * that road). A trip that reaches a port (airport) goes on from the road of another one: ports only to ports on the same
 * body of water (ships need water), airports to any airport (planes fly over land, too). Once per trip. */
static short IsHubTile(int kind, int t)
{
  t &= LOMASK;
  return (kind == 0) ? (t >= PORTBASE && t <= LASTPORT) : (t >= AIRPORTBASE && t < COALBASE);
}

RegisterHub(int kind)		/* called by the map scan for a powered port (0) or airport (1) at SMapX, SMapY */
{
  short r = (kind == 0) ? 4 : 5, x, y, n = NextHubN[kind];

  if (!TrafficRules || n >= 8) return;
  for (y = SMapY - r; y <= SMapY + r; y++)
    for (x = SMapX - r; x <= SMapX + r; x++) {
      if (!TestBounds(x, y) || !RoadTest(Map[x][y]) || IsHwy(Map[x][y])) continue;
      if ((TestBounds(x, y - 1) && IsHubTile(kind, Map[x][y - 1])) || (TestBounds(x + 1, y) && IsHubTile(kind, Map[x + 1][y])) ||
	  (TestBounds(x, y + 1) && IsHubTile(kind, Map[x][y + 1])) || (TestBounds(x - 1, y) && IsHubTile(kind, Map[x - 1][y]))) {
	NextHubX[kind][n] = SMapX; NextHubY[kind][n] = SMapY;
	NextHubRX[kind][n] = x; NextHubRY[kind][n] = y;
	NextHubN[kind] = n + 1;
	return;
      }
    }
}

static short Sea[WORLD_X][WORLD_Y];		/* water body label per tile (0 = not water / not labelled) */
static short SeaStackX[WORLD_X * WORLD_Y], SeaStackY[WORLD_X * WORLD_Y];

static short ShipTile(int t)		/* a tile a ship can sail on (the same set as the sprite code's BtClrTab, plus open and closed bridges) */
{
  t &= LOMASK;
  return (t == RIVER || t == REDGE || t == CHANNEL || t == BRWH || t == BRWV || t == HBRIDGE || t == VBRIDGE ||
	  t == POWERBASE || t == POWERBASE + 1 || t == RAILBASE || t == RAILBASE + 1 ||
	  (t >= HBRDG0 && t <= HBRDG0 + 3) || (t >= VBRDG0 && t <= VBRDG0 + 3)) && 1;
}

static short IsWaterTile(short x, short y)
{
  if (!TestBounds(x, y)) return 0;
  return ShipTile(Map[x][y]);
}

static void HubWater(void)		/* which body of water does each port touch? (recomputed when the ports change) */
{
  short k, x, y, same = (HubSeaN == HubN[0]), label = 0, sp;
  static const short DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

  for (k = 0; k < HubN[0] && same; k++) if (HubSeaFor[k][0] != HubX[0][k] || HubSeaFor[k][1] != HubY[0][k]) same = 0;
  if (same) return;
  for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++) Sea[x][y] = 0;
  for (k = 0; k < HubN[0]; k++) {
    HubSea[k] = 0;
    HubSeaFor[k][0] = HubX[0][k]; HubSeaFor[k][1] = HubY[0][k];
    for (y = HubY[0][k] - 5; y <= HubY[0][k] + 5 && !HubSea[k]; y++)		/* the nearest water tile of the port */
      for (x = HubX[0][k] - 5; x <= HubX[0][k] + 5; x++)
	if (IsWaterTile(x, y)) {
	  if (!Sea[x][y]) {							/* label this whole body of water */
	    label++; sp = 0;
	    SeaStackX[sp] = x; SeaStackY[sp] = y; sp++; Sea[x][y] = label;
	    while (sp > 0) {
	      short cx, cy, d;
	      sp--; cx = SeaStackX[sp]; cy = SeaStackY[sp];
	      for (d = 0; d < 4; d++) {
		short nx = cx + DX[d], ny = cy + DY[d];
		if (IsWaterTile(nx, ny) && !Sea[nx][ny]) { Sea[nx][ny] = label; SeaStackX[sp] = nx; SeaStackY[sp] = ny; sp++; }
	      }
	    }
	  }
	  HubSea[k] = Sea[x][y];
	  break;
	}
  }
  HubSeaN = HubN[0];
}

/* ---- ships and planes that really go from hub to hub (the trip that hopped sends them off) ---- */
#define MAXPATH 900
short ShipPathX[MAXPATH], ShipPathY[MAXPATH], ShipPathN, ShipPathI;
static short Par[WORLD_X * WORLD_Y], Que[WORLD_X * WORLD_Y];

static short NearWater(short cx, short cy, short *wx, short *wy)	/* the sailable tile nearest to a hub centre */
{
  short r, x, y;
  for (r = 1; r <= 7; r++)
    for (y = cy - r; y <= cy + r; y++)
      for (x = cx - r; x <= cx + r; x++) {
	if (x != cx - r && x != cx + r && y != cy - r && y != cy + r) continue;	/* the ring only */
	if (IsWaterTile(x, y)) { *wx = x; *wy = y; return (1); }
      }
  return (0);
}

static short BuildShipPath(short sx, short sy, short tx, short ty)	/* breadth first search over sailable tiles, 8 directions */
{
  static const short DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 }, DY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
  short head = 0, tail = 0, d, x, y, n, goal = ty * WORLD_X + tx, start = sy * WORLD_X + sx, cur;
  long i;

  for (i = 0; i < WORLD_X * WORLD_Y; i++) Par[i] = -1;
  Par[start] = start; Que[tail++] = start;
  while (head < tail && Par[goal] < 0) {
    cur = Que[head++]; x = cur % WORLD_X; y = cur / WORLD_X;
    for (d = 0; d < 8; d++) {
      short nx = x + DX[d], ny = y + DY[d];
      if (!IsWaterTile(nx, ny) || Par[ny * WORLD_X + nx] >= 0) continue;
      if ((d & 1) && !(IsWaterTile(x + DX[d], y) && IsWaterTile(x, y + DY[d]))) continue;	/* no cutting corners across land */
      Par[ny * WORLD_X + nx] = cur; Que[tail++] = ny * WORLD_X + nx;
    }
  }
  if (Par[goal] < 0) return (0);
  n = 0;
  for (cur = goal; cur != start; cur = Par[cur]) {			/* from the goal back to the start */
    if (n >= MAXPATH) return (0);
    ShipPathX[n] = cur % WORLD_X; ShipPathY[n] = cur / WORLD_X; n++;
  }
  for (i = 0; i < n / 2; i++) {						/* reverse: start first */
    short tx2 = ShipPathX[i], ty2 = ShipPathY[i];
    ShipPathX[i] = ShipPathX[n - 1 - i]; ShipPathY[i] = ShipPathY[n - 1 - i];
    ShipPathX[n - 1 - i] = tx2; ShipPathY[n - 1 - i] = ty2;
  }
  ShipPathN = n; ShipPathI = 0;
  return (1);
}

static void LaunchTransport(short kind, short a, short b)
{
  SimSprite *sp;

  if (kind == 1) {					/* a plane from airport a to airport b */
    if (GetSprite(AIR) != NULL) return;
    sp = MakeSprite(AIR, (HubX[1][a] << 4) + 48, (HubY[1][a] << 4) + 12);
    sp->dest_x = (HubX[1][b] << 4) + 48; sp->dest_y = (HubY[1][b] << 4) + 12;
    sp->flag = 1;					/* a scheduled flight: to the other airport and back, then it lands */
    sp->orig_x = sp->x; sp->orig_y = sp->y; sp->step = 0;
    TripPlanes++;
  } else {						/* a ship from port a to port b over the water */
    short sx, sy, tx, ty;
    if (GetSprite(SHI) != NULL) return;
    if (!NearWater(HubX[0][a], HubY[0][a], &sx, &sy) || !NearWater(HubX[0][b], HubY[0][b], &tx, &ty)) return;
    if (!BuildShipPath(sx, sy, tx, ty)) return;
    sp = MakeSprite(SHI, (sx << 4) + 8 - 48, (sy << 4) + 8);
    sp->flag = 1;					/* a scheduled trip: it follows ShipPath there and back (see DoShipTrip) */
    sp->step = 0;
    sp->frame = sp->new_dir = 3; sp->dir = 10; sp->count = 0;
    TripShips++;
  }
}

/* The map scan asks for a ship (port) or a plane: with the traffic rules and at least two hubs they sail / fly between hubs
 * instead of wandering. Returns 1 when a scheduled trip was started. */
short LaunchRandomTrip(short kind)
{
  short a, b, tries;

  if (!TrafficRules || HubN[kind] < 2) return (0);
  if (kind == 0) HubWater();
  for (tries = 0; tries < 6; tries++) {
    a = Rand(HubN[kind] - 1);
    b = Rand(HubN[kind] - 2); if (b >= a) b++;
    if (kind == 1 || (HubSea[a] && HubSea[a] == HubSea[b])) {
      LaunchTransport(kind, a, b);
      return (1);
    }
  }
  return (0);
}

void PlaneTrip(SimSprite *sp, int x, int y)		/* a plane that starts at the airport at x, y flies to another airport */
{
  short k, here = -1, other;
  if (!TrafficRules || HubN[1] < 2) return;
  for (k = 0; k < HubN[1]; k++)
    if (abs(HubX[1][k] - x) + abs(HubY[1][k] - y) <= 6) { here = k; break; }
  if (here < 0) return;
  other = Rand(HubN[1] - 2); if (other >= here) other++;
  sp->dest_x = (HubX[1][other] << 4) + 48; sp->dest_y = (HubY[1][other] << 4) + 12;
  sp->orig_x = sp->x; sp->orig_y = sp->y; sp->step = 0;
  sp->flag = 1;
  TripPlanes++;
}

static short HubFull(short kind, short k)			/* the hub has used up its capacity this year (warns once a year) */
{
  short s = HubStatFor(kind, HubX[kind][k], HubY[kind][k]);
  if (HubStat[s].now >= HUB_CAPACITY * 8 / 10 && HubStat[s].warned < 1 && HubStat[s].now < HUB_CAPACITY) {	/* 80 %: a warning in time */
    HubStat[s].warned = 1;
    Eval(kind == 0 ? "UISetMessage {A seaport is nearly at capacity}" : "UISetMessage {An airport is nearly at capacity}");
  }
  if (HubStat[s].now < HUB_CAPACITY) return (0);
  if (HubStat[s].warned < 2) {
    HubStat[s].warned = 2;
    Eval(kind == 0 ? "UISetMessage {A seaport is at capacity: build another one}" : "UISetMessage {An airport is at capacity: build another one}");
  }
  return (1);
}

HubHop(void)
{
  static short DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
  short d, k, kind, here = -1, found = 0, cand[8], nc = 0;

  for (d = 0; d < 4 && !found; d++) {
    short tx = SMapX + DX[d], ty = SMapY + DY[d];
    if (!TestBounds(tx, ty)) continue;
    if (IsHubTile(0, Map[tx][ty])) { kind = 0; found = 1; }
    else if (IsHubTile(1, Map[tx][ty])) { kind = 1; found = 1; }
  }
  if (!found || HubN[kind] < 2) return;
  for (k = 0; k < HubN[kind]; k++)		/* which hub are we at? the one whose road we are on or next to */
    if (abs(HubRX[kind][k] - SMapX) + abs(HubRY[kind][k] - SMapY) <= 8) { here = k; break; }
  if (here < 0) return;
  if (kind == 0) HubWater();
  if (HubFull(kind, here)) return;				/* this hub has carried all it can this year */
  for (k = 0; k < HubN[kind]; k++)
    if (k != here && (kind == 1 || (HubSea[k] && HubSea[k] == HubSea[here]))) {
      if (HubFull(kind, k)) continue;				/* a full partner is no way on */
      cand[nc++] = k;
    }
  if (!nc) return;
  k = cand[Rand(nc - 1)];
  SMapX = HubRX[kind][k]; SMapY = HubRY[kind][k]; LDir = 5; Hopped = 1; HopCount++;	/* the positions of the first leg stay on the stack: they get their traffic, too */
  HubStat[HubStatFor(kind, HubX[kind][here], HubY[kind][here])].now++;
  HubStat[HubStatFor(kind, HubX[kind][k], HubY[kind][k])].now++;
  if (!(Rand16() & 3)) LaunchTransport(kind, here, k);	/* the plane / ship that carries it, now and then */
}


/* ---- what the query tool shows about traffic (UIShowTrafficStatus) ---- */
static void ZoneAccess(short cx, short cy, short *rail, short *jam)	/* like FindPRoad: a rail tile first, else the road in front */
{
  static short PX[12] = {-1, 0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2};
  static short PY[12] = {-2,-2,-2,-1, 0, 1, 2, 2, 2, 1, 0,-1};
  short z, tx, ty, t;
  *rail = 0; *jam = 0;
  for (z = 0; z < 12; z++) {
    tx = cx + PX[z]; ty = cy + PY[z];
    if (!TestBounds(tx, ty)) continue;
    t = Map[tx][ty] & LOMASK;
    if (t >= RAILBASE && t <= LASTRAIL) { *rail = 1; return; }
  }
  for (z = 0; z < 12; z++) {
    tx = cx + PX[z]; ty = cy + PY[z];
    if (TestBounds(tx, ty) && RoadTest(Map[tx][ty]) && !IsHwy(Map[tx][ty])) { *jam = TrfDensity[tx >>1][ty >>1]; return; }
  }
}

void TrafficStatus(short x, short y)				/* sent after UIShowZoneStatus for the tile that was queried */
{
  char buf[400], *p = buf;
  short t, cx, cy, found = 0, k, kind = -1, n = 0, i, j;
  if (!TestBounds(x, y)) return;
  t = Map[x][y] & LOMASK;

  p += sprintf(p, "UIShowTrafficStatus");
  if (!TrafficRules || !TestBounds(x, y)) { Eval(buf); return; }
  if (IsHwy(t)) {
    p += sprintf(p, " {Traffic|Highway}");
  } else if (t >= ROADBASE && t < POWERBASE) {
    p += sprintf(p, " {Traffic|%d}", (int)TrfDensity[x >>1][y >>1]);
  } else if (t >= RESBASE && t < COALBASE) {
    short r = (t < PORTBASE) ? 1 : (t < AIRPORTBASE) ? 2 : 3, best = 99, ct;	/* how far the centre tile can be from any tile of the zone */
    for (cy = y - r; cy <= y + r; cy++)
      for (cx = x - r; cx <= x + r; cx++) {
	if (!TestBounds(cx, cy) || !(Map[cx][cy] & ZONEBIT)) continue;
	ct = Map[cx][cy] & LOMASK;
	if (ct < RESBASE || ct >= COALBASE) continue;
	if (abs(cx - x) + abs(cy - y) < best) { best = abs(cx - x) + abs(cy - y); k = cx; n = cy; found = 1; }
      }
    if (found) {
      cx = k; cy = n; n = 0;
      ct = Map[cx][cy] & LOMASK;
      if (ct >= RESBASE && ct < PORTBASE) {					/* a zone: residential, commercial, industrial */
	short rail, jam, eff;
	ZoneAccess(cx, cy, &rail, &jam);
	eff = (jam > 100 ? -(jam - 100) * 6 : 0);			/* the rail bonus (up to 150) depends on how much of the trips goes by rail */
	p += sprintf(p, " {Traffic|%d}", (int)jam);
	p += sprintf(p, " {Rail link|%s}", rail ? "Yes" : "No");
	p += sprintf(p, " {Traffic effect|%+d}", (int)eff);
      } else {									/* a port or an airport */
	kind = (ct < AIRPORTBASE) ? 0 : 1;
	for (k = 0; k < HubN[kind]; k++)
	  if (abs(HubX[kind][k] - cx) <= 1 && abs(HubY[kind][k] - cy) <= 1) break;
	if (k < HubN[kind]) {							/* registered: powered and with a road */
	  short s = HubStatFor(kind, HubX[kind][k], HubY[kind][k]);
	  if (kind == 0) HubWater();
	  for (j = 0; j < HubN[kind]; j++) if (j != k && (kind == 1 || (HubSea[j] && HubSea[j] == HubSea[k]))) n++;
	  p += sprintf(p, " {Linked hubs|%d}", (int)n);
	  p += sprintf(p, " {Used this year|%d of %d}", (int)HubStat[s].now, HUB_CAPACITY);
	  p += sprintf(p, " {Used last year|%d of %d}", (int)HubStat[s].last, HUB_CAPACITY);
	} else {
	  short rr = (kind == 0) ? 4 : 5, road = 0;
	  for (j = cy - rr; j <= cy + rr && !road; j++)			/* a road touching the hub (as RegisterHub looks for it) */
	    for (i = cx - rr; i <= cx + rr; i++) {
	      if (!TestBounds(i, j) || !RoadTest(Map[i][j]) || IsHwy(Map[i][j])) continue;
	      if ((TestBounds(i, j - 1) && IsHubTile(kind, Map[i][j - 1])) || (TestBounds(i + 1, j) && IsHubTile(kind, Map[i + 1][j])) ||
		  (TestBounds(i, j + 1) && IsHubTile(kind, Map[i][j + 1])) || (TestBounds(i - 1, j) && IsHubTile(kind, Map[i - 1][j]))) { road = 1; break; }
	    }
	  p += sprintf(p, " {Power|%s}", (Map[cx][cy] & PWRBIT) ? "Yes" : "No");
	  p += sprintf(p, " {Road link|%s}", road ? "Yes" : "No");
	  if ((Map[cx][cy] & PWRBIT) && road) p += sprintf(p, " {Linked hubs|-}");		/* the next map scan registers it */
	}
      }
    }
  }
  Eval(buf);
}

/* A trip that wandered 30 steps without finding its destination may still reach a port or airport: a distance field over the
 * road net (multi-source breadth first search from the roads at the ports / airports that have a partner) tells every road tile
 * the way to the nearest one. The trip walks down the field, loading the roads on the way, hops, and wanders on from there. */
#define FIELD_AGE 4
static unsigned char HubField[2][WORLD_X * WORLD_Y];
static long HubFieldTime = -1000;

static void BuildHubField(void)
{
  static const short DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
  short kind, k, head, tail, x, y, d, partner;
  long i;

  if (CityTime - HubFieldTime < FIELD_AGE && CityTime >= HubFieldTime) return;
  HubFieldTime = CityTime;
  for (kind = 0; kind < 2; kind++) {
    for (i = 0; i < WORLD_X * WORLD_Y; i++) HubField[kind][i] = 255;
    if (HubN[kind] < 2) continue;
    if (kind == 0) HubWater();
    head = tail = 0;
    for (k = 0; k < HubN[kind]; k++) {
      partner = (kind == 1);
      if (!partner) for (d = 0; d < HubN[0]; d++) if (d != k && HubSea[d] && HubSea[d] == HubSea[k]) partner = 1;
      if (!partner || HubFull(kind, k)) continue;
      i = HubRX[kind][k] * WORLD_Y + HubRY[kind][k];
      if (HubField[kind][i] == 0) continue;
      HubField[kind][i] = 0; Que[tail++] = (short)i;
    }
    while (head < tail) {
      short cur = Que[head++], cx = cur / WORLD_Y, cy = cur % WORLD_Y, v = HubField[kind][cur];
      if (v >= 250) continue;
      for (d = 0; d < 4; d++) {
	x = cx + DX[d]; y = cy + DY[d];
	if (!TestBounds(x, y) || !RoadTest(Map[x][y])) continue;
	i = x * WORLD_Y + y;
	if (HubField[kind][i] != 255) continue;
	HubField[kind][i] = v + 1; Que[tail++] = (short)i;
      }
    }
  }
}

/* Road areas: how many homes, shops and factories does a connected piece of the road net serve? A trip only goes on over a hub
 * when its own area is short of destinations for it (an island of industry with few or no homes, homes without work, ...): a
 * trip that merely wandered off the wrong way in a balanced town just fails as in the original, it does not turn into a stream
 * of trips to the port. */
static short RoadComp[WORLD_X * WORLD_Y];
static unsigned char CompRes[WORLD_X * WORLD_Y / 4 + 8], CompCom[WORLD_X * WORLD_Y / 4 + 8], CompInd[WORLD_X * WORLD_Y / 4 + 8];	/* zones per area (saturating) */
static long CompTime = -1000;

static void BuildRoadAreas(void)
{
  static const short DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
  short x, y, d, head, tail, label = 0, cx, cy, r, t, xx, yy, found, lab;
  long i;

  if (CityTime - CompTime < FIELD_AGE && CityTime >= CompTime) return;
  CompTime = CityTime;
  for (i = 0; i < WORLD_X * WORLD_Y; i++) RoadComp[i] = 0;
  for (x = 0; x < WORLD_X; x++)
    for (y = 0; y < WORLD_Y; y++) {
      i = x * WORLD_Y + y;
      if (RoadComp[i] || !RoadTest(Map[x][y]) || label >= WORLD_X * WORLD_Y / 4) continue;
      label++; CompRes[label] = CompCom[label] = CompInd[label] = 0; head = tail = 0;
      RoadComp[i] = label; Que[tail++] = (short)i;
      while (head < tail) {
	short cur = Que[head++];
	cx = cur / WORLD_Y; cy = cur % WORLD_Y;
	for (d = 0; d < 4; d++) {
	  xx = cx + DX[d]; yy = cy + DY[d];
	  if (!TestBounds(xx, yy) || !RoadTest(Map[xx][yy]) || RoadComp[xx * WORLD_Y + yy]) continue;
	  RoadComp[xx * WORLD_Y + yy] = label; Que[tail++] = (short)(xx * WORLD_Y + yy);
	}
      }
    }
  for (x = 0; x < WORLD_X; x++)
    for (y = 0; y < WORLD_Y; y++) {
      if (!(Map[x][y] & ZONEBIT)) continue;
      t = Map[x][y] & LOMASK;
      if (t < RESBASE || t > NUCLEAR) continue;
      r = (t < PORTBASE) ? 2 : 4;					/* how far a road can be from the centre tile */
      found = 0; lab = 0;
      for (yy = y - r; yy <= y + r && !found; yy++)
	for (xx = x - r; xx <= x + r; xx++)
	  if (TestBounds(xx, yy) && RoadComp[xx * WORLD_Y + yy]) { lab = RoadComp[xx * WORLD_Y + yy]; found = 1; break; }
      if (!found) continue;
      if (t < COMBASE) { if (CompRes[lab] < 255) CompRes[lab]++; }				/* homes */
      else if (t < INDBASE) { if (CompCom[lab] < 255) CompCom[lab]++; }
      else if (t < PORTBASE) { if (CompInd[lab] < 255) CompInd[lab]++; }
    }
}

static short IsolatedFor(int Zt, short sx, short sy)		/* the zone's area has no destination for its trips, so a hub is the only way */
{
  short lab;
  if (Zt == 1) return (0);					/* commercial trips find a target next to themselves */
  BuildRoadAreas();
  lab = RoadComp[sx * WORLD_Y + sy];
  if (!lab) return (0);
  if (Zt == 0) return (3 * (CompCom[lab] + CompInd[lab]) < CompRes[lab]);	/* homes without enough shops and factories to work in */
  return (2 * CompRes[lab] < CompInd[lab]);			/* factories without enough homes (less than one home zone for two factories) */
}

static short HubTrip(short sx, short sy)
{
  static const short DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
  short kind, d, x = sx, y = sy, steps, best, bx, by, z;
  long i = sx * WORLD_Y + sy;

  if (HubN[0] < 2 && HubN[1] < 2) return (0);
  BuildHubField();
  kind = (HubField[0][i] <= HubField[1][i]) ? 0 : 1;
  if (HubField[kind][i] >= 250) return (0);			/* no hub reachable from here */
  for (steps = 0; steps < 120 && HubField[kind][x * WORLD_Y + y] > 0; steps++) {
    best = HubField[kind][x * WORLD_Y + y]; bx = -1; by = -1;
    for (d = 0; d < 4; d++) {
      short nx = x + DX[d], ny = y + DY[d];
      if (TestBounds(nx, ny) && RoadTest(Map[nx][ny]) && HubField[kind][nx * WORLD_Y + ny] < best) { best = HubField[kind][nx * WORLD_Y + ny]; bx = nx; by = ny; }
    }
    if (bx < 0) return (0);					/* the road has changed since the field was made */
    x = bx; y = by;
    z = Map[x][y] & LOMASK;
    if (z >= RAILBASE && z <= LASTRAIL) RailSteps++; else RoadSteps++;
    if (z >= ROADBASE && z < POWERBASE) {			/* load the road on the way, like SetTrafMem */
      short dn = TrfDensity[x >>1][y >>1] + HUB_LOAD;
      if (dn > 240) dn = 240;
      TrfDensity[x >>1][y >>1] = dn;
    }
  }
  if (HubField[kind][x * WORLD_Y + y] != 0) return (0);
  SMapX = x; SMapY = y; Hopped = 0; PosStackN = 0;
  HubHop();							/* jumps to a partner hub's road */
  if (!Hopped) return (0);
  LDir = 5;
  if (TryDrive()) {						/* wander on from there */
    SetTrafMem();
    return (1);
  }
  return (0);
}


/* comefrom: TryGo FindPRoad */
RoadTest(int x)
{
  x = x & LOMASK;
  if (x >= HWYBASE && x <= HWYLAST)
    return (TrafficRules);			/* highways are part of the road net when the traffic rules are on */
  if (x < ROADBASE)
    return (FALSE);
  if (x > LASTRAIL)
    return (FALSE);
  if ((x >= POWERBASE) && (x < RAILHPOWERV))
    return (FALSE);
  return (TRUE);
}

