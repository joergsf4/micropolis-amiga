/* Import of cities from the original SimCity (Amiga, Atari ST, Mac).
 *
 * The original SimCity for the Amiga stores its cities (and the scenarios on its disk) in the format of the Mac version,
 * wrapped in a MacBinary header: 128 bytes (name of the city, file type "CITY", creator "MCRP", length of the data),
 * then exactly the 27120 bytes of a Micropolis .cty file, then padding to a multiple of 128 bytes. The simulation core only
 * accepts files of exactly 27120 bytes, so the data part is copied to a file in T: and loaded from there; the core is not
 * changed. Tested with cities saved by the Amiga and the Atari ST version. */
#include <stdio.h>
#include <string.h>
#include "sim.h"
#include "ui.h"
#include "simcity_import.h"

int setAnyCityName(char *name);                 /* w_util.c (K&R, no prototype in sim.h) */

#define MB_HEAD 128
#define CTY_SIZE 27120L

static long be32(const unsigned char *p) { return ((long)p[0] << 24) | ((long)p[1] << 16) | ((long)p[2] << 8) | p[3]; }

/* is this a MacBinary header of a SimCity city? name: the city name from the header */
static int macbinary_city(const unsigned char *h, long file_size, char *name, size_t n)
{
    if (h[0] != 0 || h[1] < 1 || h[1] > 63 || h[74] != 0 || h[82] != 0) return 0;     /* MacBinary: version 0, name 1..63 */
    if (memcmp(h + 65, "CITY", 4)) return 0;                                         /* file type of a SimCity city */
    if (be32(h + 83) != CTY_SIZE || be32(h + 87) != 0) return 0;                      /* data fork: one city; no resources */
    if (file_size < MB_HEAD + CTY_SIZE) return 0;
    if (n) { size_t l = h[1] < n ? h[1] : n - 1; memcpy(name, h + 2, l); name[l] = 0; }
    return 1;
}

int city_load_any(const char *path)
{
    static char name[64];
    static char buf[CTY_SIZE];
    unsigned char head[MB_HEAD];
    const char *tmp = "T:Micropolis-import.cty";
    FILE *f, *o;
    long size;
    int is_mb;
    char p[256];

    if (!(f = fopen(path, "rb"))) return CITY_FAILED;
    fseek(f, 0L, SEEK_END); size = ftell(f); fseek(f, 0L, SEEK_SET);
    is_mb = size >= MB_HEAD && fread(head, 1, MB_HEAD, f) == MB_HEAD && macbinary_city(head, size, name, sizeof name);
    if (!is_mb) {                                                       /* a normal Micropolis city */
        fclose(f);
        if (size != CTY_SIZE && size != 99120L && size != 219120L) return CITY_FAILED;   /* the sizes the core knows */
        strncpy(p, path, sizeof p - 1); p[sizeof p - 1] = 0;
        return LoadCity(p) ? CITY_LOADED : CITY_FAILED;
    }
    if (fread(buf, 1, CTY_SIZE, f) != (size_t)CTY_SIZE) { fclose(f); return CITY_FAILED; }
    fclose(f);
    if (!(o = fopen(tmp, "wb"))) { tmp = "RAM:Micropolis-import.cty"; if (!(o = fopen(tmp, "wb"))) return CITY_FAILED; }
    if (fwrite(buf, 1, CTY_SIZE, o) != (size_t)CTY_SIZE) { fclose(o); remove(tmp); return CITY_FAILED; }
    fclose(o);
    strcpy(p, tmp);
    if (!LoadCity(p)) { remove(tmp); return CITY_FAILED; }
    remove(tmp);
    if (CityFileName) { ckfree(CityFileName); CityFileName = NULL; }  /* not a .cty file: "Save City" must ask for a name */
    {   char *nm = (char *)ckalloc(strlen(name) + 1);
        strcpy(nm, name);
        setAnyCityName(nm);                                             /* "San Francisco", not the name of the temporary file */
    }
    return CITY_IMPORTED;
}
