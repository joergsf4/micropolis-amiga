/* Sound effects through audio.device. The samples (files in res/sounds, made by tools/convert_assets.py from the
 * original wav files) are raw signed 8 bit mono at 11025 Hz. They must be in Chip RAM, so they are loaded when first
 * played and dropped again when the cache grows. Each sound is played on a left and a right Paula channel. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "sound.h"

#define RATE 11025
#define PERIOD (3546895 / RATE)            /* PAL colour clock; NTSC machines play about 1.3 % slower, nobody will hear it */
#define MAX_CACHE 24
#define CACHE_BYTES (300 * 1024)

struct cached { char name[32]; UBYTE *data; ULONG len; int busy; };
static struct cached cache[MAX_CACHE];
static int ncache; static ULONG cache_size;

static struct MsgPort *port;
static struct IOAudio *io[2];              /* left, right */
static int open_ok[2], in_flight[2];
static struct cached *playing[2];

static int open_channel(int side)
{
    static UBYTE maps[2][2] = { { 1, 8 }, { 2, 4 } };       /* left: channels 0 or 3, right: channels 1 or 2 */
    io[side] = (struct IOAudio *)CreateIORequest(port, sizeof(struct IOAudio));
    if (!io[side]) return 0;
    io[side]->ioa_Request.io_Message.mn_Node.ln_Pri = 20;
    io[side]->ioa_AllocKey = 0;
    io[side]->ioa_Data = maps[side];
    io[side]->ioa_Length = 2;
    if (OpenDevice((CONST_STRPTR)"audio.device", 0, (struct IORequest *)io[side], 0)) {
        DeleteIORequest((struct IORequest *)io[side]); io[side] = NULL;
        return 0;
    }
    return 1;
}

void sound_init(void)
{
    port = CreateMsgPort();
    if (!port) return;
    open_ok[0] = open_channel(0);
    open_ok[1] = open_channel(1);
}

static void reap(int side)
{
    if (in_flight[side] && CheckIO((struct IORequest *)io[side])) {
        WaitIO((struct IORequest *)io[side]);
        in_flight[side] = 0;
        if (playing[side]) playing[side]->busy--;
        playing[side] = NULL;
    }
}

void sound_poll(void) { reap(0); reap(1); }

static void drop_idle(void)
{
    int j = 0;
    for (int i = 0; i < ncache; i++) {
        if (cache[i].busy > 0) { cache[j++] = cache[i]; continue; }
        cache_size -= cache[i].len;
        FreeVec(cache[i].data);
    }
    ncache = j;
}

static struct cached *load(const char *name)
{
    char path[80];
    BPTR f;
    LONG len;
    struct cached *c;
    for (int i = 0; i < ncache; i++) if (!strcmp(cache[i].name, name)) return &cache[i];
    sprintf(path, "PROGDIR:res/sounds/%.40s.snd", name);
    f = Open((CONST_STRPTR)path, MODE_OLDFILE);
    if (!f) return NULL;
    Seek(f, 0, OFFSET_END);
    len = Seek(f, 0, OFFSET_BEGINNING);
    if (len <= 0 || len > 100000) { Close(f); return NULL; }
    if (ncache >= MAX_CACHE || cache_size + len > CACHE_BYTES) drop_idle();
    if (ncache >= MAX_CACHE) { Close(f); return NULL; }
    c = &cache[ncache];
    c->data = AllocVec(len, MEMF_CHIP);
    if (!c->data || Read(f, c->data, len) != len) { if (c->data) FreeVec(c->data); Close(f); return NULL; }
    Close(f);
    strncpy(c->name, name, 31); c->name[31] = 0;
    c->len = len; c->busy = 0;
    ncache++; cache_size += len;
    return c;
}

void sound_play(const char *id)
{
    char name[40];
    struct cached *c;
    int i;
    if (!port || !(open_ok[0] || open_ok[1])) return;
    for (i = 0; id[i] && i < 38; i++) name[i] = (id[i] >= 'A' && id[i] <= 'Z') ? id[i] + 32 : id[i];
    name[i] = 0;
    sound_poll();
    if (!(c = load(name))) return;
    for (int side = 0; side < 2; side++) {
        struct IOAudio *a = io[side];
        if (!open_ok[side]) continue;
        if (in_flight[side]) {                          /* a sound is still running on this channel: newest wins */
            AbortIO((struct IORequest *)a); WaitIO((struct IORequest *)a);
            in_flight[side] = 0; if (playing[side]) playing[side]->busy--; playing[side] = NULL;
        }
        a->ioa_Request.io_Command = CMD_WRITE;
        a->ioa_Request.io_Flags = ADIOF_PERVOL;
        a->ioa_Data = c->data;
        a->ioa_Length = c->len;
        a->ioa_Period = PERIOD;
        a->ioa_Volume = 64;
        a->ioa_Cycles = 1;
        SendIO((struct IORequest *)a);
        in_flight[side] = 1; playing[side] = c; c->busy++;
    }
}

void sound_exit(void)
{
    for (int side = 0; side < 2; side++) {
        if (io[side]) {
            if (in_flight[side]) { AbortIO((struct IORequest *)io[side]); WaitIO((struct IORequest *)io[side]); }
            CloseDevice((struct IORequest *)io[side]);
            DeleteIORequest((struct IORequest *)io[side]);
            io[side] = NULL;
        }
    }
    if (port) { DeleteMsgPort(port); port = NULL; }
    for (int i = 0; i < ncache; i++) FreeVec(cache[i].data);
    ncache = 0; cache_size = 0;
}
