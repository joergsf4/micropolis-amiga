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

void sound_poll(void) {}

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

void sound_play(const char *id)
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
    {   struct dtTrigger t;
        t.MethodID = DTM_TRIGGER; t.dtt_GInfo = NULL; t.dtt_Function = STM_PLAY; t.dtt_Data = NULL;
        DoDTMethodA(o, NULL, NULL, (Msg)&t);
        trigger_count++;
    }
}

void sound_exit(void)
{
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
