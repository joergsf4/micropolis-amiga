/* Overview map (the "small map" of the original, 3x3 pixels per tile) with the overlays of
 * upstream g_map.c / g_smmaps.c: all, residential, commercial, industrial, power, roads,
 * population density, growth, traffic, pollution, crime, land value, fire and police coverage. */
#define GetSprite sim_GetSprite
#include "sim.h"
#include "ui.h"
#undef GetSprite
#include <stdio.h>
#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include "mapview.h"

int map_visible;
int map_mode;

const char *const map_names[MAP_MODES] = {
    "All", "Residential", "Commercial", "Industrial", "Power grid", "Roads and rails",
    "Population density", "Growth rate", "Traffic", "Pollution", "Crime", "Land value",
    "Fire coverage", "Police coverage"
};

static UBYTE *smalltiles;          /* 960 tiles * 4 x 3 palette indices (first 3 of each row used) */
static UBYTE mapbuf[MV_W * MV_H];

int map_load(void)
{
    BPTR f = Open((CONST_STRPTR)"res/tilessm.bin", MODE_OLDFILE);
    if (!f) return 0;
    smalltiles = AllocVec(TILE_COUNT * 12, MEMF_ANY);
    if (!smalltiles || Read(f, smalltiles, TILE_COUNT * 12) != TILE_COUNT * 12) {
        Close(f);
        return 0;
    }
    Close(f);
    return 1;
}

/* value classes of upstream g_map.c GetCI(): none/low/medium/high/very high */
static int ci(int x)
{
    if (x < 50) return 0;
    if (x < 100) return 1;
    if (x < 150) return 2;
    if (x < 200) return 3;
    return 4;
}

/* class -> palette index (see ui.h): none, low, medium, high, very high, plus, very plus, minus, very minus */
static const UBYTE val_pen[9] = { 0, PEN_V_LGRAY, PEN_V_YELLOW, PEN_V_ORANGE, PEN_V_RED,
                                  PEN_V_DGREEN, PEN_V_LGREEN, PEN_V_ORANGE, PEN_V_YELLOW };

static void put_tile(int col, int row, int tile)
{
    const UBYTE *src = smalltiles + tile * 12;
    UBYTE *dst = mapbuf + row * MAP_PX * MV_W + col * MAP_PX;
    for (int y = 0; y < 3; y++, src += 4, dst += MV_W) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; }
}

static void fill3(int col, int row, UBYTE pen)
{
    UBYTE *dst = mapbuf + row * MAP_PX * MV_W + col * MAP_PX;
    for (int y = 0; y < 3; y++, dst += MV_W) { dst[0] = dst[1] = dst[2] = pen; }
}

/* the tile-filter modes of g_smmaps.c */
static void draw_tiles(int mode)
{
    for (int col = 0; col < WORLD_X; col++) {
        for (int row = 0; row < WORLD_Y; row++) {
            unsigned tile = (unsigned short)Map[col][row] & LOMASK;
            if (tile >= TILE_COUNT) tile -= TILE_COUNT;
            switch (mode) {
            case 1: if (tile > 422) tile = 0;
                break;                                                                    /* residential */
            case 2: if (tile > 609 || (tile >= 232 && tile < 423)) tile = 0;
                break;                                                                    /* commercial */
            case 3:                                                                       /* industrial */
                if ((tile >= 240 && tile <= 611) || (tile >= 693 && tile <= 851) ||
                    (tile >= 860 && tile <= 883) || tile >= 932) tile = 0;
                break;
            case 5: case 8:                                                               /* roads, traffic base */
                if (tile >= 240 || (tile >= 207 && tile <= 220) || tile == 223) tile = 0;
                break;
            }
            put_tile(col, row, tile);
        }
    }
}

static void draw_power(void)
{
    for (int col = 0; col < WORLD_X; col++) {
        for (int row = 0; row < WORLD_Y; row++) {
            unsigned short tile = (unsigned short)Map[col][row];
            if ((tile & LOMASK) >= TILE_COUNT) tile -= TILE_COUNT;
            if ((tile & LOMASK) <= 63) put_tile(col, row, tile & LOMASK);
            else if (tile & ZONEBIT) fill3(col, row, (tile & PWRBIT) ? PEN_V_RED : PEN_V_LBLUE);
            else if (tile & CONDBIT) fill3(col, row, PEN_V_LGRAY);
            else put_tile(col, row, 0);
        }
    }
}

/* stippled rectangle like upstream drawRect(): every other pixel */
static void rect(int cls, int x, int y, int w, int h)
{
    if (cls == 0) return;
    UBYTE pen = val_pen[cls];
    for (int j = 0; j < h && y + j < MV_H; j++)
        for (int i = 0; i < w && x + i < MV_W; i++)
            if (((x + i + y + j) & 1) == 0) mapbuf[(y + j) * MV_W + x + i] = pen;
}

static void overlay_2x2(Byte *const *arr, int bias)
{
    for (int x = 0; x < HWLDX; x++)
        for (int y = 0; y < HWLDY; y++) rect(ci(bias + arr[x][y]), x * 6, y * 6, 6, 6);
}

static void overlay_8x8(const short *arr, int stride_y)
{
    for (int x = 0; x < SmX; x++)
        for (int y = 0; y < SmY; y++) rect(ci(arr[x * stride_y + y]), x * 24, y * 24, 24, 24);
}

void map_refresh(void)
{
    if (!smalltiles) return;
    switch (map_mode) {
    case 4: draw_power(); return;
    case 5: case 8: draw_tiles(5); break;
    case 0: case 1: case 2: case 3: draw_tiles(map_mode); return;
    default: draw_tiles(0);
    }
    switch (map_mode) {
    case 6:  overlay_2x2(PopDensity, 0); break;
    case 7:                                                   /* growth: plus / minus classes */
        for (int x = 0; x < SmX; x++)
            for (int y = 0; y < SmY; y++) {
                int z = RateOGMem[x][y], v = 0;
                if (z > 100) v = 6; else if (z > 20) v = 5; else if (z < -100) v = 8; else if (z < -20) v = 7;
                rect(v, x * 24, y * 24, 24, 24);
            }
        break;
    case 8:  overlay_2x2(TrfDensity, 0); break;
    case 9:  overlay_2x2(PollutionMem, 10); break;
    case 10: overlay_2x2(CrimeMem, 0); break;
    case 11: overlay_2x2(LandValueMem, 0); break;
    case 12: overlay_8x8((const short *)FireRate, SmY); break;
    case 13: overlay_8x8((const short *)PoliceMapEffect, SmY); break;
    }
}

void map_blit(UBYTE *fb, int stride, int x0, int y0, int view_x, int view_y, int cols, int rows)
{
    if (!smalltiles) return;
    for (int y = 0; y < MV_H; y++) memcpy(fb + (y0 + y) * stride + x0, mapbuf + y * MV_W, MV_W);
    /* frame and the part of the city visible in the main view */
    UBYTE c = PEN_TEXT;
    int vx = x0 + view_x * MAP_PX, vy = y0 + view_y * MAP_PX, vw = cols * MAP_PX, vh = rows * MAP_PX;
    for (int i = 0; i < vw; i++) { fb[vy * stride + vx + i] = c; fb[(vy + vh - 1) * stride + vx + i] = c; }
    for (int j = 0; j < vh; j++) { fb[(vy + j) * stride + vx] = c; fb[(vy + j) * stride + vx + vw - 1] = c; }
    for (int i = -1; i <= MV_W; i++) { fb[(y0 - 1) * stride + x0 + i] = c; fb[(y0 + MV_H) * stride + x0 + i] = c; }
    for (int j = -1; j <= MV_H; j++) { fb[(y0 + j) * stride + x0 - 1] = c; fb[(y0 + j) * stride + x0 + MV_W] = c; }
}

int map_hit(int x, int y, int x0, int y0, int *tx, int *ty)
{
    if (x < x0 || y < y0 || x >= x0 + MV_W || y >= y0 + MV_H) return 0;
    *tx = (x - x0) / MAP_PX;
    *ty = (y - y0) / MAP_PX;
    return 1;
}

const UBYTE *map_buffer(void) { return mapbuf; }   /* for the headless test */
