/* Replaces the X11/Tcl/Tk/POSIX include block of upstream sim.h on AmigaOS.
 * The view structs in view.h keep their fields; their window-system types are dummies. */
#ifndef AMIGA_COMPAT_H
#define AMIGA_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

/* window-system types used by the view structs (never dereferenced in the core) */
typedef void *Display, *Screen, *Visual, *GC, *XImage, *XFontStruct, *Tk_Window, *Tcl_Interp,
             *Tk_3DBorder, *Tk_TimerToken, *TkDisplay, *XShmSegmentInfo;
typedef unsigned long Window, Colormap, Pixmap, Time;
typedef struct { short x, y; } XPoint;

/* upstream calls the Tcl UI through Eval(); the Amiga frontend implements this */
int Eval(char *cmd);

extern int tkMustExit;
#include <signal.h>
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGINT
#define SIGINT 2
#endif

/* Unix hands out zeroed pages for big mallocs and upstream relies on it; AmigaOS does not */
#define ckalloc(n) calloc(1, (n))
#define ckfree(p) free(p)
#define None 0L

#define Tk_PathName(w) ((char *)"view")

#define XSetClipMask(...) ((void)0)
#define XSetClipOrigin(...) ((void)0)
#define XCopyArea(...) ((void)0)

#endif
