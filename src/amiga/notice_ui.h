#ifndef NOTICE_UI_H
#define NOTICE_UI_H
#include <exec/types.h>
extern int notice_visible;
#define NOTICE_X 100                         /* panel rectangle incl. border */
#define NOTICE_Y 90
#define NOTICE_W 444
#define NOTICE_H 300
int  notice_load(void);                      /* res/notices.txt */
void notice_show(int id);                    /* "UIShowPicture id" and the win/lose notices; ignores unknown ids */
void notice_draw(UBYTE *fb, int stride);     /* panel graphics into the frame buffer */
void notice_text(void);                      /* panel text into the frame buffer (gfx_target must be set), after notice_draw */
int  notice_click(int x, int y);             /* returns 1 when handled (closes on click inside the button) */
void notice_close(void);
#endif
