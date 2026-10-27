/* Sound effects through the system's sound.datatype (the same path the Workbench uses for the Sound preferences
 * "Test" button and for playing sound files). The samples are IFF 8SVX files (res/sounds/<id>.8svx, made by
 * tools/convert_assets.py from the original wav files: 8 bit mono, 11025 Hz). The datatype takes care of Chip RAM,
 * cache flushing and the audio.device channels. Direct audio.device writes did not produce sound on the Workbench 3.2
 * test machine, the datatype did. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include "sound.h"
#include "l10n.h"
#include "stack.h"             /* app_log() */
#include "sound_ahi.h"

#define MAX_OBJ 20

/* Paula has four channels. Sound effects and the spoken announcements must never be cut by an ambient loop, so:
 * at most two loops play at once, and a loop is only started (or started again) while no effect is playing. */
static struct timeval fx_end;                          /* when the last started effect is over */
static int now_after(const struct timeval *t)
{
    struct timeval n; gettimeofday(&n, NULL);
    return n.tv_sec > t->tv_sec || (n.tv_sec == t->tv_sec && n.tv_usec >= t->tv_usec);
}
static void fx_busy(Object *o)                         /* remember how long this effect keeps its channel */
{
    ULONG len = 0, rate = 0;
    struct timeval n;
    GetDTAttrs(o, SDTA_SampleLength, (ULONG)&len, SDTA_SamplesPerSec, (ULONG)&rate, TAG_DONE);
    if (!rate) rate = 11025;
    gettimeofday(&n, NULL);
    n.tv_sec += len / rate; n.tv_usec += (len % rate) * 1000000UL / rate + 150000;      /* + a little safety margin */
    if (n.tv_usec >= 1000000) { n.tv_sec++; n.tv_usec -= 1000000; }
    if (n.tv_sec > fx_end.tv_sec || (n.tv_sec == fx_end.tv_sec && n.tv_usec > fx_end.tv_usec)) fx_end = n;
}

struct snd { char name[32]; Object *obj; };
static struct snd cache[MAX_OBJ];
static int ncache;
struct Library *DataTypesBase;
static int played_count, failed_load, trigger_count;
int sound_fx_vol = 100;                                /* 0..100: volume of the sound effects (slider in the sound window) */
int sound_amb_vol = 50;                                /* 0..100: volume of the ambient loops */
int sound_output;                                      /* 0 = the system (sound.datatype, Paula), 1..4 = AHI unit 0..3 */
static int ahi_missing;                                /* the last try to open AHI failed */

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
    if (ahi_is_open()) {                                   /* AHI: lower case name, no channel games needed */
        char nm[40]; int k;
        for (k = 0; id[k] && k < 38; k++) nm[k] = (id[k] >= 'A' && id[k] <= 'Z') ? id[k] + 32 : id[k];
        nm[k] = 0;
        ahi_effect(nm, sound_fx_vol);
        return;
    }
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
    SetDTAttrs(o, NULL, NULL, SDTA_Volume, (ULONG)(sound_fx_vol * 64 / 100), TAG_DONE);
    {   struct dtTrigger t;
        t.MethodID = DTM_TRIGGER; t.dtt_GInfo = NULL; t.dtt_Function = STM_PLAY; t.dtt_Data = NULL;
        DoDTMethodA(o, NULL, NULL, (Msg)&t);
        trigger_count++;
        fx_busy(o);
    }
}

/* --- ambient loops: sea, city, nature; only their volumes change (see ambient_set / ambient_tick) --- */
enum { AMB_SEA, AMB_CITY, AMB_NATURE, AMB_N };
static const char *const amb_file[AMB_N] = { "amb_sea", "amb_city", "amb_nature" };
static Object *amb_obj[AMB_N];
static int amb_vol[AMB_N], amb_target[AMB_N], amb_tick_no;
static BYTE amb_sig[AMB_N] = { -1, -1, -1 };                  /* signal bits: the datatype signals when a loop has ended */
static int amb_restarts;

static void amb_trigger(Object *o, ULONG fn)
{
    struct dtTrigger t;
    t.MethodID = DTM_TRIGGER; t.dtt_GInfo = NULL; t.dtt_Function = fn; t.dtt_Data = NULL;
    DoDTMethodA(o, NULL, NULL, (Msg)&t);
}

void ambient_set(int sea, int city, int nature)              /* wanted volumes 0..64 */
{
    amb_target[AMB_SEA] = sea < 3 ? 0 : sea; amb_target[AMB_CITY] = city < 3 ? 0 : city; amb_target[AMB_NATURE] = nature < 3 ? 0 : nature;   /* nothing audible is not worth a channel */
}

static void ambient_tick_ahi(void)                             /* the same fading, played by AHI: no channel conflicts, no restarts */
{
    int n = 0, weakest = -1;
    for (int i = 0; i < AMB_N; i++) if (amb_target[i] > 0) { n++; if (weakest < 0 || amb_target[i] < amb_target[weakest]) weakest = i; }
    if (n > 2) amb_target[weakest] = 0;                       /* two loops are enough, and keep the mix calm */
    { static int svc; if (++svc % 3 == 0) ahi_service(); }      /* the chunks last 0.7 s: looking after them every third frame is enough (less processor time) */
    if (++amb_tick_no & 1) return;
    for (int i = 0; i < AMB_N; i++) {
        int v = amb_vol[i];
        if (v < amb_target[i]) v++; else if (v > amb_target[i]) v--;
        if (v > 0 && !ahi_loop_active(i)) { if (!ahi_loop_start(i, amb_file[i], v)) { amb_target[i] = 0; v = 0; } }
        else if (v == 0 && ahi_loop_active(i)) ahi_loop_stop(i);
        else if (ahi_loop_active(i)) ahi_loop_volume(i, v);
        amb_vol[i] = v;
    }
}

void ambient_tick(void)                                       /* once per frame: move the volumes towards the targets (about 1 s from silence to 25) */
{
    if (ahi_is_open()) { ambient_tick_ahi(); return; }
    if (!DataTypesBase) return;
    int fx_free = now_after(&fx_end);
    {   int n = 0, weakest = -1;                              /* at most two loops: the weakest of three is silenced */
        for (int i = 0; i < AMB_N; i++) if (amb_target[i] > 0) { n++; if (weakest < 0 || amb_target[i] < amb_target[weakest]) weakest = i; }
        if (n > 2) amb_target[weakest] = 0;
    }
    for (int i = 0; i < AMB_N; i++) {                         /* a loop that ended by itself (channel taken by a sound effect, ...) starts again at once */
        if (amb_sig[i] < 0) continue;
        if ((SetSignal(0, 1UL << amb_sig[i]) & (1UL << amb_sig[i])) && amb_obj[i] && amb_vol[i] > 0) {
            if (!fx_free) { SetSignal(1UL << amb_sig[i], 1UL << amb_sig[i]); continue; }       /* later: not while an effect plays */
            amb_trigger(amb_obj[i], STM_PLAY);
            if (++amb_restarts <= 20) app_log("ambient: loop had ended, started again");
        }
    }
    if (++amb_tick_no & 1) return;
    for (int i = 0; i < AMB_N; i++) {
        int v = amb_vol[i];
        if (v < amb_target[i]) v++; else if (v > amb_target[i]) v--;
        if (v == amb_vol[i] && (v > 0) == (amb_obj[i] != NULL)) continue;
        if (v > 0 && !amb_obj[i] && !fx_free) continue;           /* a new loop waits until the effect is over */
        if (v > 0 && !amb_obj[i]) {                           /* a loop that becomes audible: load and start it (its samples use Chip RAM only while it plays) */
            char path[80];
            sprintf(path, "PROGDIR:res/sounds/%s.8svx", amb_file[i]);
            if (amb_sig[i] < 0) amb_sig[i] = AllocSignal(-1);
            amb_obj[i] = NewDTObject((CONST_STRPTR)path, DTA_SourceType, DTST_FILE, DTA_GroupID, GID_SOUND,
                                     SDTA_Volume, v, SDTA_Cycles, 0,
                                     amb_sig[i] >= 0 ? SDTA_SignalTask : TAG_IGNORE, (ULONG)FindTask(NULL),
                                     amb_sig[i] >= 0 ? SDTA_SignalBitMask : TAG_IGNORE, amb_sig[i] >= 0 ? 1UL << amb_sig[i] : 0, TAG_DONE);
            if (amb_sig[i] >= 0) SetSignal(0, 1UL << amb_sig[i]);
            if (!amb_obj[i]) { amb_target[i] = 0; v = 0; app_log("ambient: cannot load a loop"); }
            else { amb_trigger(amb_obj[i], STM_PLAY); app_log("ambient: loop started"); }
        } else if (v == 0 && amb_obj[i]) {                    /* silent again: stop it and give the memory back */
            amb_trigger(amb_obj[i], STM_STOP);
            DisposeDTObject(amb_obj[i]); amb_obj[i] = NULL;
            app_log("ambient: loop stopped");
        } else if (amb_obj[i]) {
            SetDTAttrs(amb_obj[i], NULL, NULL, SDTA_Volume, (ULONG)v, TAG_DONE);
        }
        amb_vol[i] = v;
    }
}

void ambient_off(void)
{
    ambient_set(0, 0, 0);
    for (int i = 0; i < AMB_N; i++) if (ahi_loop_active(i)) ahi_loop_stop(i);
    for (int i = 0; i < AMB_N; i++) {
        if (amb_obj[i]) { amb_trigger(amb_obj[i], STM_STOP); DisposeDTObject(amb_obj[i]); amb_obj[i] = NULL; }
        amb_vol[i] = 0;
    }
}

/* ---- the output: the system (sound.datatype) or AHI ---- */
int sound_set_output(int sel)                                  /* 0 = system, 1..4 = AHI unit 0..3; returns 1 when it is in use now */
{
    ambient_off();
    if (ahi_is_open()) ahi_close();
    ahi_missing = 0;
    sound_output = 0;
    if (sel >= 1 && sel <= 4) {
        if (ahi_open(sel - 1)) sound_output = sel;
        else { ahi_missing = 1; app_log("sound: ahi.device unit cannot be opened, staying with the system sound"); }
    }
    return sound_output == sel;
}

int sound_cycle_output(int dir)                                /* the button of the sound window: system, AHI unit 0, 1, 2, 3, system ... (dir -1: the other way round) */
{
    int next = (sound_output + dir + 5) % 5;
    while (next > 0 && !sound_set_output(next)) next = (next + dir + 5) % 5;   /* a unit that cannot be opened is skipped */
    if (next == 0) { int missing = ahi_missing; sound_set_output(0); ahi_missing = missing; }
    return sound_output;
}

const char *sound_output_label(char *buf)
{
    if (sound_output > 0) sprintf(buf, T("Output: AHI unit %d"), sound_output - 1);
    else if (ahi_missing) strcpy(buf, T("Output: AHI not available (system)"));
    else strcpy(buf, T("Output: system (Paula)"));
    return buf;
}

void sound_exit(void)
{
    ambient_off();
    if (ahi_is_open()) ahi_close();
    for (int i = 0; i < AMB_N; i++) if (amb_sig[i] >= 0) { FreeSignal(amb_sig[i]); amb_sig[i] = -1; }
    for (int i = 0; i < ncache; i++) DisposeDTObject(cache[i].obj);
    ncache = 0;
    if (DataTypesBase) { CloseLibrary(DataTypesBase); DataTypesBase = NULL; }
}

const char *sound_status(char *buf)            /* one line for the About box */
{
    if (ahi_is_open()) sprintf(buf, "Sound: AHI unit %d, %d played", ahi_unit(), ahi_played());
    else if (!DataTypesBase) sprintf(buf, "%s", T("Sound: datatypes.library missing"));
    else sprintf(buf, T("Sound: datatype, %d played, %d not loadable"), played_count, failed_load);
    return buf;
}
