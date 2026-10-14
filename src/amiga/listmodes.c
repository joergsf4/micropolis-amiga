/* Lists all display modes the system knows (id, name, size, depth) - for debugging RTG screenmodes. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/graphics.h>

struct GfxBase *GfxBase;

int main(void)
{
    ULONG id = INVALID_ID;
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!GfxBase) return 20;
    while ((id = NextDisplayInfo(id)) != INVALID_ID) {
        struct NameInfo ni;
        struct DimensionInfo di;
        char name[40] = "?";
        if (GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof ni, DTAG_NAME, id) > 0) {
            strncpy(name, (char *)ni.Name, 31); name[31] = 0;
        }
        if (GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof di, DTAG_DIMS, id) > 0)
            printf("%08lx  %-32s  %4dx%-4d depth %d\n", (unsigned long)id, name,
                   (int)(di.Nominal.MaxX - di.Nominal.MinX + 1), (int)(di.Nominal.MaxY - di.Nominal.MinY + 1), (int)di.MaxDepth);
        else
            printf("%08lx  %-32s\n", (unsigned long)id, name);
    }
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
