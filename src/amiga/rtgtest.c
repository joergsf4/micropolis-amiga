/* Milestone 0: open a cybergraphics (RTG) screen and draw into it. */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

struct Library *CyberGfxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    if (!IntuitionBase || !GfxBase) return 20;
    if (!CyberGfxBase) { printf("cybergraphics.library fehlt (Picasso96 nicht aktiv)\n"); return 10; }

    ULONG id = BestCModeIDTags(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480,
                               CYBRBIDTG_Depth, 8, TAG_DONE);
    printf("BestCModeID = 0x%08lx, IsCyberModeID = %ld\n", (unsigned long)id,
           id == INVALID_ID ? 0L : (long)IsCyberModeID(id));
    if (id == INVALID_ID) { printf("kein RTG-Modus gefunden\n"); return 10; }

    struct Screen *scr = OpenScreenTags(NULL, SA_DisplayID, id, SA_Width, 640, SA_Height, 480,
                                        SA_Depth, 8, SA_Title, (ULONG)"RTG Test", SA_ShowTitle, FALSE,
                                        TAG_DONE);
    if (!scr) { printf("OpenScreen fehlgeschlagen\n"); return 10; }

    ULONG pal[2 + 256 * 3];
    pal[0] = (256UL << 16) | 0;
    for (int i = 0; i < 256; i++) {
        pal[1 + i * 3] = (ULONG)i * 0x01010101UL;
        pal[2 + i * 3] = (ULONG)(255 - i) * 0x01010101UL;
        pal[3 + i * 3] = 0x80808080UL;
    }
    pal[1 + 256 * 3] = 0;
    LoadRGB32(&scr->ViewPort, pal);

    UBYTE *line = AllocVec(640, MEMF_ANY);
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 640; x++) line[x] = (UBYTE)((x * 256 / 640 + y) & 255);
        WritePixelArray(line, 0, 0, 640, &scr->RastPort, 0, y, 640, 1, RECTFMT_LUT8);
    }
    FreeVec(line);

    Delay(50 * 6);   /* 6 seconds */
    CloseScreen(scr);
    CloseLibrary(CyberGfxBase);
    return 0;
}
