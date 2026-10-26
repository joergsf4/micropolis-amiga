/* Sound output through AHI (ahi.device): the effects and the three ambient loops are mixed by AHI, so they never fight over
 * Paula's four channels and a sound card with an AHI driver can be used. The samples are the game's 8SVX files (8 bit signed mono,
 * 11025 Hz) which are read here without the sound datatype. A loop is played as a stream of short chunks that follow each other
 * gaplessly (ahir_Link), so its volume can follow the map within a fraction of a second. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "sound_ahi.h"
#include "stack.h"             /* app_log() */

#define FX_REQS 6              /* sound effects that can play at the same time */
#define MAX_SND 24             /* cached effect files */
#define CHUNK 4096             /* samples per loop chunk (0.37 s) */
#define MIXVOL 0x10000L        /* AHI's "1.0" */

struct snd { char name[32]; UBYTE *buf, *pcm; ULONG len, rate; };

static struct MsgPort *port;
static struct AHIRequest *master;
static int unit_open = -1;
static struct AHIRequest *fxreq[FX_REQS];
static UBYTE fxbusy[FX_REQS];
static struct snd cache[MAX_SND];
static int ncache, played;

struct loop { struct snd s; struct AHIRequest *rq[2]; UBYTE busy[2]; ULONG pos; long vol; int active; };
static struct loop loops[AHI_LOOPS];

static void snd_free(struct snd *s) { if (s->buf) FreeVec(s->buf); memset(s, 0, sizeof *s); }

static int snd_load(const char *name, struct snd *s)           /* reads PROGDIR:res/sounds/<name>.8svx: FORM 8SVX with VHDR and BODY */
{
    char path[96];
    BPTR f;
    LONG size;
    UBYTE *b;
    ULONG i;
    sprintf(path, "PROGDIR:res/sounds/%.40s.8svx", name);
    if (!(f = Open((CONST_STRPTR)path, MODE_OLDFILE))) return 0;
    Seek(f, 0, OFFSET_END); size = Seek(f, 0, OFFSET_BEGINNING);
    if (size < 40 || !(b = AllocVec((ULONG)size, MEMF_PUBLIC))) { Close(f); return 0; }
    if (Read(f, b, size) != size) { Close(f); FreeVec(b); return 0; }
    Close(f);
    if (memcmp(b, "FORM", 4) || memcmp(b + 8, "8SVX", 4)) { FreeVec(b); return 0; }
    memset(s, 0, sizeof *s);
    s->rate = 11025;
    for (i = 12; i + 8 <= (ULONG)size; ) {
        ULONG len = ((ULONG)b[i + 4] << 24) | ((ULONG)b[i + 5] << 16) | ((ULONG)b[i + 6] << 8) | b[i + 7];
        if (!memcmp(b + i, "VHDR", 4) && len >= 14) s->rate = ((ULONG)b[i + 8 + 12] << 8) | b[i + 8 + 13];
        if (!memcmp(b + i, "BODY", 4)) { s->pcm = b + i + 8; s->len = len; if (i + 8 + len > (ULONG)size) s->len = (ULONG)size - i - 8; break; }
        i += 8 + len + (len & 1);
    }
    if (!s->pcm || !s->len) { FreeVec(b); return 0; }
    s->buf = b;
    strncpy(s->name, name, 31);
    return 1;
}

static struct AHIRequest *new_req(void)
{
    struct AHIRequest *r = (struct AHIRequest *)AllocVec(sizeof(struct AHIRequest), MEMF_PUBLIC | MEMF_CLEAR);
    if (r) { CopyMem(master, r, sizeof(struct AHIRequest)); r->ahir_Std.io_Message.mn_ReplyPort = port; }
    return r;
}

static void send(struct AHIRequest *r, const UBYTE *data, ULONG len, ULONG rate, long vol, struct AHIRequest *link, int pri)
{
    r->ahir_Std.io_Message.mn_Node.ln_Pri = (BYTE)pri;
    r->ahir_Std.io_Command = CMD_WRITE;
    r->ahir_Std.io_Data = (APTR)data;
    r->ahir_Std.io_Length = len;
    r->ahir_Std.io_Offset = 0;
    r->ahir_Type = AHIST_M8S;
    r->ahir_Frequency = rate;
    r->ahir_Volume = vol < 0 ? 0 : vol > MIXVOL ? MIXVOL : vol;
    r->ahir_Position = 0x8000;
    r->ahir_Link = link;
    SendIO((struct IORequest *)r);
}

static int done(struct AHIRequest *r, UBYTE *busy)             /* a finished request is taken off the port and is free again */
{
    if (!*busy) return 1;
    if (!CheckIO((struct IORequest *)r)) return 0;
    WaitIO((struct IORequest *)r);
    *busy = 0;
    return 1;
}

int ahi_open(int unit)
{
    if (unit_open >= 0) ahi_close();
    if (!(port = CreateMsgPort())) return 0;
    if (!(master = (struct AHIRequest *)CreateIORequest(port, sizeof(struct AHIRequest)))) { ahi_close(); return 0; }
    master->ahir_Version = 4;
    if (OpenDevice((CONST_STRPTR)"ahi.device", (ULONG)unit, (struct IORequest *)master, 0)) {
        DeleteIORequest((struct IORequest *)master); master = NULL;
        DeleteMsgPort(port); port = NULL;
        return 0;
    }
    unit_open = unit;
    for (int i = 0; i < FX_REQS; i++) fxreq[i] = new_req();
    for (int l = 0; l < AHI_LOOPS; l++) for (int k = 0; k < 2; k++) loops[l].rq[k] = new_req();
    app_log("sound: ahi.device open");
    return 1;
}

void ahi_close(void)
{
    if (unit_open < 0 && !port) return;
    for (int l = 0; l < AHI_LOOPS; l++) ahi_loop_stop(l);
    for (int i = 0; i < FX_REQS; i++) if (fxreq[i]) {
        if (fxbusy[i]) { AbortIO((struct IORequest *)fxreq[i]); WaitIO((struct IORequest *)fxreq[i]); fxbusy[i] = 0; }
        FreeVec(fxreq[i]); fxreq[i] = NULL;
    }
    for (int l = 0; l < AHI_LOOPS; l++) for (int k = 0; k < 2; k++) if (loops[l].rq[k]) { FreeVec(loops[l].rq[k]); loops[l].rq[k] = NULL; }
    for (int i = 0; i < ncache; i++) snd_free(&cache[i]);
    ncache = 0;
    if (master) { if (unit_open >= 0) CloseDevice((struct IORequest *)master); DeleteIORequest((struct IORequest *)master); master = NULL; }
    if (port) { DeleteMsgPort(port); port = NULL; }
    unit_open = -1;
}

int ahi_is_open(void) { return unit_open >= 0; }
int ahi_unit(void) { return unit_open; }
int ahi_played(void) { return played; }

void ahi_effect(const char *name, int vol100)
{
    struct snd *s = NULL;
    int i, slot = -1;
    if (unit_open < 0) return;
    for (i = 0; i < ncache; i++) if (!strcmp(cache[i].name, name)) { s = &cache[i]; break; }
    if (!s) {
        if (ncache >= MAX_SND) {                                 /* cache full: drop everything that is not playing (none of them: the requests copy nothing, but may still read) */
            for (i = 0; i < FX_REQS; i++) if (fxbusy[i]) { AbortIO((struct IORequest *)fxreq[i]); WaitIO((struct IORequest *)fxreq[i]); fxbusy[i] = 0; }
            for (i = 0; i < ncache; i++) snd_free(&cache[i]);
            ncache = 0;
        }
        if (!snd_load(name, &cache[ncache])) return;
        s = &cache[ncache++];
    }
    for (i = 0; i < FX_REQS; i++) if (done(fxreq[i], &fxbusy[i])) { slot = i; break; }
    if (slot < 0) return;                                          /* all busy: this effect is skipped */
    send(fxreq[slot], s->pcm, s->len, s->rate, (long)vol100 * MIXVOL / 100, NULL, 0);
    fxbusy[slot] = 1;
    played++;
}

int ahi_loop_start(int l, const char *name, int vol64)
{
    struct loop *p = &loops[l];
    if (unit_open < 0 || p->active) return 0;
    if (!snd_load(name, &p->s)) return 0;
    p->pos = 0; p->vol = (long)vol64 * MIXVOL / 64; p->active = 1;
    p->busy[0] = p->busy[1] = 0;
    return 1;
}

void ahi_loop_volume(int l, int vol64) { loops[l].vol = (long)vol64 * MIXVOL / 64; }

void ahi_loop_stop(int l)
{
    struct loop *p = &loops[l];
    for (int k = 0; k < 2; k++) if (p->busy[k]) { AbortIO((struct IORequest *)p->rq[k]); WaitIO((struct IORequest *)p->rq[k]); p->busy[k] = 0; }
    if (p->active) snd_free(&p->s);
    p->active = 0;
}

int ahi_loop_active(int l) { return loops[l].active; }

void ahi_service(void)                                             /* once per frame: keep two chunks of every loop in flight */
{
    if (unit_open < 0) return;
    for (int l = 0; l < AHI_LOOPS; l++) {
        struct loop *p = &loops[l];
        if (!p->active) continue;
        for (int k = 0; k < 2; k++) done(p->rq[k], &p->busy[k]);
        for (int k = 0; k < 2; k++) {
            ULONG n;
            if (p->busy[k]) continue;
            n = p->s.len - p->pos;
            if (n > CHUNK) n = CHUNK;
            send(p->rq[k], p->s.pcm + p->pos, n, p->s.rate, p->vol, p->busy[k ^ 1] ? p->rq[k ^ 1] : NULL, -20);   /* gapless after the other chunk */
            p->busy[k] = 1;
            p->pos += n;
            if (p->pos >= p->s.len) p->pos = 0;
        }
    }
}
