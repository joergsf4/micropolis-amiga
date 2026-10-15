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
#include <sys/time.h>
#include "sound.h"
#include "stack.h"             /* app_log() */

#define RATE 11025
#define PERIOD (3546895 / RATE)            /* PAL colour clock; NTSC machines play about 1.3 % slower, nobody will hear it */
#define MAX_CACHE 24
#define CACHE_BYTES (300 * 1024)

struct cached { char name[32]; UBYTE *data; ULONG len; int busy; };
static struct cached cache[MAX_CACHE];
static int ncache; static ULONG cache_size;

static struct MsgPort *port;
#define NCH 4                              /* all four Paula channels: 0 and 3 are left, 1 and 2 are right; two per side = twice as loud */
static struct IOAudio *io[NCH];
static int open_ok[NCH], in_flight[NCH];
static int open_err[NCH];                 /* OpenDevice result per channel (0 = ok) */
static int played_count, done_ok, done_err, last_err;   /* completed writes, and how they ended */
static struct cached *playing[NCH];
static struct timeval started[NCH];      /* when each write was sent: to check that playback takes as long as the sample */

static int open_channel(int side)
{
    static UBYTE maps[NCH][1] = { { 1 }, { 8 }, { 2 }, { 4 } };   /* one channel each: 0, 3 (left), 1, 2 (right) */
    io[side] = (struct IOAudio *)CreateIORequest(port, sizeof(struct IOAudio));
    if (!io[side]) return 0;
    io[side]->ioa_Request.io_Message.mn_Node.ln_Pri = 20;
    io[side]->ioa_AllocKey = 0;
    io[side]->ioa_Data = maps[side];
    io[side]->ioa_Length = 1;
    {   BYTE err = OpenDevice((CONST_STRPTR)"audio.device", 0, (struct IORequest *)io[side], 0);
        char lb[80];
        open_err[side] = (int)err;
        sprintf(lb, "sound: audio.device channel %d: OpenDevice error %d", side, (int)err);
        app_log(lb);
        if (err) {
            DeleteIORequest((struct IORequest *)io[side]); io[side] = NULL;
            return 0;
        }
    }
    return 1;
}

void sound_init(void)
{
    port = CreateMsgPort();
    if (!port) { for (int i = 0; i < NCH; i++) open_err[i] = 999; return; }
    for (int i = 0; i < NCH; i++) open_ok[i] = open_channel(i);
}

static void reap(int side)
{
    if (in_flight[side] && CheckIO((struct IORequest *)io[side])) {
        WaitIO((struct IORequest *)io[side]);
        if (io[side]->ioa_Request.io_Error) {
            char lb[80];
            done_err++; last_err = io[side]->ioa_Request.io_Error;
            sprintf(lb, "sound: channel %d write ended with error %d", side, last_err);
            app_log(lb);
        } else {
            struct timeval now;
            long ms;
            gettimeofday(&now, NULL);
            ms = (now.tv_sec - started[side].tv_sec) * 1000L + (now.tv_usec - started[side].tv_usec) / 1000L;
            done_ok++;
            if (playing[side]) {
                char lb[100];
                sprintf(lb, "sound: ch %d done after %ld ms (sample %ld ms)", side, ms, (long)(playing[side]->len * 1000L / RATE));
                app_log(lb);
            }
        }
        in_flight[side] = 0;
        if (playing[side]) playing[side]->busy--;
        playing[side] = NULL;
    }
}

void sound_poll(void) { for (int i = 0; i < NCH; i++) reap(i); }

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
    CacheClearU();                       /* on 68040/060 the CPU cache may hold the sample: flush before Paula reads it from Chip RAM */
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
    if (!port || !(open_ok[0] || open_ok[1] || open_ok[2] || open_ok[3])) { app_log("sound: no audio channel, not playing"); return; }
    for (i = 0; id[i] && i < 38; i++) name[i] = (id[i] >= 'A' && id[i] <= 'Z') ? id[i] + 32 : id[i];
    name[i] = 0;
    sound_poll();
    if (!(c = load(name))) {
        char lb[100]; sprintf(lb, "sound: cannot load %s", name); app_log(lb);
        return;
    }
    { char lb[100]; sprintf(lb, "sound: play %s (%ld bytes)", name, (long)c->len); app_log(lb); }
    played_count++;
    for (int side = 0; side < NCH; side++) {
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
        gettimeofday(&started[side], NULL);
        SendIO((struct IORequest *)a);
        in_flight[side] = 1; playing[side] = c; c->busy++;
    }
}

void sound_exit(void)
{
    for (int side = 0; side < NCH; side++) {
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

const char *sound_status(char *buf)            /* one line for the About box */
{
    int n = 0;
    for (int i = 0; i < NCH; i++) n += open_ok[i];
    sound_poll();
    if (n == NCH) sprintf(buf, "Sound: 4 ch, %d played, %d done, %d failed (%d)", played_count, done_ok, done_err, last_err);
    else if (n) sprintf(buf, "Sound: %d of 4 channels, %d played", n, played_count);
    else sprintf(buf, "Sound: audio.device unavailable (error %d)", open_err[0]);
    return buf;
}
