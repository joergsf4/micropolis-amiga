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
short ZoneTested;		/* MakeTraf ran for this zone in this pass */
short HubX[2][8], HubY[2][8], HubN[2];		/* ports (0) and airports (1) with power: zone centre tiles (of the last complete map scan) */
short NextHubX[2][8], NextHubY[2][8], NextHubN[2];	/* the scan in progress */
static short Hopped;
long HopCount;			/* trips that went on over a port or airport (statistics) */


/* comefrom: DoIndustrial DoCommercial DoResidential */
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
    if (TryDrive()) {		/* attempt to drive somewhere */
      if (!(TrafficRules && ZoneOnRail))
	SetTrafMem();		/* if sucessful, inc trafdensity (a trip on rails does not load the roads) */
      SMapX = xtem;
      SMapY = ytem;
      return (TRUE);		/* traffic passed */
    }
    SMapX = xtem;
    SMapY = ytem;
    return (FALSE);		/* traffic failed */
  }
  else return (-1);		/* no road found */
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
      if (TrafficRules && IsHwy(Map[SMapX][SMapY]) && free < 200) {	/* a highway is fast: its tiles do not use up the distance */
	z--; free++;
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


/* Ports and airports link separated areas: a trip that reaches a port (airport) goes on from the road next to another
 * powered port (airport), as ships (planes) would carry it. Once per trip. */
HubHop(void)
{
  static short DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
  short d, k, kind, n, here = -1, to, cx, cy, x, y, found = 0;

  for (d = 0; d < 4 && !found; d++) {
    short tx = SMapX + DX[d], ty = SMapY + DY[d], t;
    if (!TestBounds(tx, ty)) continue;
    t = Map[tx][ty] & LOMASK;
    if (t >= PORTBASE && t <= LASTPORT) { kind = 0; found = 1; }
    else if (t >= AIRPORTBASE && t < COALBASE) { kind = 1; found = 1; }
  }
  if (!found || HubN[kind] < 2) return;
  for (k = 0; k < HubN[kind]; k++) {		/* which hub are we at? the nearest centre */
    short ax = HubX[kind][k] - SMapX, ay = HubY[kind][k] - SMapY;
    if (ax < 0) ax = -ax;
    if (ay < 0) ay = -ay;
    if (ax + ay <= 6) { here = k; break; }
  }
  if (here < 0) return;
  n = HubN[kind] - 1;
  to = Rand(n);
  if (to >= here) to++;
  cx = HubX[kind][to]; cy = HubY[kind][to];
  for (y = cy - 4; y <= cy + 4 && !Hopped; y++)
    for (x = cx - 4; x <= cx + 4; x++)
      if (TestBounds(x, y) && RoadTest(Map[x][y])) {
	SMapX = x; SMapY = y; LDir = 5; PosStackN = 0; Hopped = 1; HopCount++;
	break;
      }
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

