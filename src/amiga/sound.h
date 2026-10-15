#ifndef SOUND_H
#define SOUND_H
void sound_init(void);                     /* opens audio.device, silent when it is not available */
void sound_play(const char *id);           /* id as in the core's "UIMakeSound <channel> <id>"; plays res/sounds/<id>.snd */
void sound_poll(void);                     /* call once per frame: reaps finished sounds */
void ambient_set(int sea, int city, int nature);   /* wanted volumes 0..64 of the three ambient loops */
void ambient_tick(void);                   /* call once per frame: fades the loops towards the wanted volumes */
void ambient_off(void);
void sound_exit(void);
const char *sound_status(char *buf);       /* text for the About box */
#endif
