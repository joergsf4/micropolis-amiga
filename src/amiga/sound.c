/* Sound effects through the system's sound.datatype (the same path the Workbench uses for the Sound preferences
 * "Test" button and for playing sound files). The samples are IFF 8SVX files (res/sounds/<id>.8svx, made by
 * tools/convert_assets.py from the original wav files: 8 bit mono, 11025 Hz). The datatype takes care of Chip RAM,
 * cache flushing and the audio.device channels. Direct audio.device writes did not produce sound on the Workbench 3.2
 * test machine, the datatype did. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include "sound.h"
#include "stack.h"             /* app_log() */

#define MAX_OBJ 20

struct snd { char name[32]; Object *obj; };
static struct snd cache[MAX_OBJ];
static int ncache;
struct Library *DataTypesBase;
static int played_count, failed_load, trigger_count;

void sound_init(void)
{
    DataTypesBase = OpenLibrary((CONST_STRPTR)"datatypes.library", 39);
    app_log(DataTypesBase ? "sound: datatypes.library open" : "sound: datatypes.library missing, no sound");
}

static Object *loop_obj;                        /* running bulldozer loop */
struct later { char name[24]; int pct; unsigned long due; int used; };
static struct later queue[4];

static unsigned long now_ticks(void)            /* 1/50 s */
{
    struct DateStamp ds;
    DateStamp(&ds);
    return (unsigned long)ds.ds_Minute * 3000UL + (unsigned long)ds.ds_Tick;
}

static Object *load(const char *name)
{
    char path[80];
    Object *o;
    for (int i = 0; i < ncache; i++) if (!strcmp(cache[i].name, name)) return cache[i].obj;
    if (ncache >= MAX_OBJ) {                           /* cache full: drop everything (sounds are short) */
        for (int i = 0; i < ncache; i++) DisposeDTObject(cache[i].obj);
        ncache = 0;
    }
    sprintf(path, "PROGDIR:res/sounds/%.40s.8svx", name);
    o = NewDTObject((CONST_STRPTR)path, DTA_SourceType, DTST_FILE, DTA_GroupID, GID_SOUND,
                    SDTA_Volume, 64, SDTA_Cycles, 1, TAG_DONE);
    if (!o) return NULL;
    strncpy(cache[ncache].name, name, 31); cache[ncache].name[31] = 0;
    cache[ncache].obj = o;
    ncache++;
    return o;
}

static void play_pitch(const char *id, int pct)
{
    char name[40];
    Object *o;
    int i;
    if (!DataTypesBase) return;
    for (i = 0; id[i] && i < 38; i++) name[i] = (id[i] >= 'A' && id[i] <= 'Z') ? id[i] + 32 : id[i];
    name[i] = 0;
    if (!(o = load(name))) {
        char lb[100]; sprintf(lb, "sound: cannot load %s", name); app_log(lb);
        failed_load++;
        return;
    }
    {   char lb[100]; sprintf(lb, "sound: play %s", name); app_log(lb); }
    played_count++;
    {   ULONG base = 0;
        GetDTAttrs(o, SDTA_SamplesPerSec, (ULONG)&base, TAG_DONE);
        if (!base) base = 11025;
        SetDTAttrs(o, NULL, NULL, SDTA_SamplesPerSec, base * (ULONG)pct / 100, TAG_DONE);
    }
    {   struct dtTrigger t;
        t.MethodID = DTM_TRIGGER; t.dtt_GInfo = NULL; t.dtt_Function = STM_PLAY; t.dtt_Data = NULL;
        DoDTMethodA(o, NULL, NULL, (Msg)&t);
        trigger_count++;
    }
}

void sound_play(const char *id) { play_pitch(id, 100); }

void sound_play_later(const char *id, int pct, int delay_ms)   /* played from sound_poll once the time has come */
{
    for (int i = 0; i < 4; i++) if (!queue[i].used) {
        strncpy(queue[i].name, id, 23); queue[i].name[23] = 0;
        queue[i].pct = pct; queue[i].due = now_ticks() + (unsigned long)delay_ms / 20; queue[i].used = 1;
        return;
    }
}

void sound_poll(void)
{
    unsigned long t;
    int any = 0;
    for (int i = 0; i < 4; i++) if (queue[i].used) { any = 1; break; }
    if (!any) return;
    t = now_ticks();
    for (int i = 0; i < 4; i++)
        if (queue[i].used && (long)(t - queue[i].due) >= 0) { queue[i].used = 0; play_pitch(queue[i].name, queue[i].pct); }
}

void sound_loop(int on)                         /* continuous bulldozer noise */
{
    struct dtTrigger t;
    if (!DataTypesBase) return;
    if (on && !loop_obj) {
        loop_obj = NewDTObject((CONST_STRPTR)"PROGDIR:res/sounds/bulldozer.8svx", DTA_SourceType, DTST_FILE, DTA_GroupID, GID_SOUND,
                               SDTA_Volume, 48, SDTA_Cycles, 0, TAG_DONE);
        if (!loop_obj) return;
        t.MethodID = DTM_TRIGGER; t.dtt_GInfo = NULL; t.dtt_Function = STM_PLAY; t.dtt_Data = NULL;
        DoDTMethodA(loop_obj, NULL, NULL, (Msg)&t);
    } else if (!on && loop_obj) {
        t.MethodID = DTM_TRIGGER; t.dtt_GInfo = NULL; t.dtt_Function = STM_STOP; t.dtt_Data = NULL;
        DoDTMethodA(loop_obj, NULL, NULL, (Msg)&t);
        DisposeDTObject(loop_obj); loop_obj = NULL;
    }
}

void sound_exit(void)
{
    sound_loop(0);
    for (int i = 0; i < ncache; i++) DisposeDTObject(cache[i].obj);
    ncache = 0;
    if (DataTypesBase) { CloseLibrary(DataTypesBase); DataTypesBase = NULL; }
}

const char *sound_status(char *buf)            /* one line for the About box */
{
    if (!DataTypesBase) sprintf(buf, "Sound: datatypes.library missing");
    else sprintf(buf, "Sound: datatype, %d played, %d not loadable", played_count, failed_load);
    return buf;
}
