#ifndef SOUND_UI_H
#define SOUND_UI_H
#include <exec/types.h>
/* Sound window: switches for the sound effects, the ambient sounds and the spoken tool names, and sliders for the
 * volume of effects and ambient sounds. */
extern int sound_visible;
extern int ambient_on;                        /* background sounds that follow the visible part of the map */
extern int speak_tools;                       /* spoken tool names when a tool is chosen */
#define SND_X 0
#define SND_Y 0
#define SND_W 360
#define SND_H 292
void sound_ui_draw(UBYTE *fb, int stride);    /* panel graphics and text (gfx_target is set by the caller) */
int  sound_ui_mouse(int x, int y, int kind);  /* kind 0 = button down, 1 = mouse moved, 2 = button up; returns 1 when the picture changes */
void sound_ui_close(void);
#endif
