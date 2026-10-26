#ifndef SOUND_AHI_H
#define SOUND_AHI_H
/* Sound output through AHI (ahi.device); see sound_ahi.c */
#define AHI_LOOPS 3
int  ahi_open(int unit);                                  /* 1 = ahi.device unit opened */
void ahi_close(void);
int  ahi_is_open(void);
int  ahi_unit(void);
int  ahi_played(void);
void ahi_effect(const char *name, int vol100);           /* plays res/sounds/<name>.8svx at the volume 0..100 */
int  ahi_loop_start(int l, const char *name, int vol64); /* ambient loop l, volume 0..64 */
void ahi_loop_volume(int l, int vol64);
void ahi_loop_stop(int l);
int  ahi_loop_active(int l);
void ahi_service(void);                                   /* once per frame */
#endif
