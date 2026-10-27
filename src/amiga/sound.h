#ifndef SOUND_H
#define SOUND_H
extern int sound_fx_vol, sound_amb_vol;     /* 0..100 */
extern int sound_output;                   /* 0 = system sound, 1..4 = AHI unit 0..3 */
int  sound_set_output(int sel);            /* 1 when the wanted output is in use */
int  sound_cycle_output(int dir);          /* next (dir 1) or previous (dir -1) output: system, AHI units that open; returns the one in use */
const char *sound_output_label(char *buf);
void sound_init(void);                     /* opens audio.device, silent when it is not available */
void sound_play(const char *id);           /* id as in the core's "UIMakeSound <channel> <id>"; plays res/sounds/<id>.snd */
void sound_poll(void);                     /* call once per frame: reaps finished sounds */
void ambient_set(int sea, int city, int nature);   /* wanted volumes 0..64 of the three ambient loops */
void ambient_tick(void);                   /* call once per frame: fades the loops towards the wanted volumes */
void ambient_off(void);
void sound_exit(void);
const char *sound_status(char *buf);       /* text for the About box */
#endif
