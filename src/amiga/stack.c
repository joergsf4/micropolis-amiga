/* The upstream core keeps large buffers on the stack, but the shell/Workbench stack is only 4 KB and
 * libnix' __stack variable is not honoured by this toolchain (StackSwap() crashed on the way back).
 * So the real main() runs in a child process with its own big stack; the parent waits for it. */
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "stack.h"

static int (*child_fn)(int, char **);
static int child_argc;
static char **child_argv;
static int child_rc = 20;
static struct Task *parent_task;
static ULONG parent_sig;

static void child_entry(void)
{
    child_rc = child_fn(child_argc, child_argv);
    Forbid();                      /* parent must not run (and unload this code) before we are gone */
    Signal(parent_task, parent_sig);
}

int run_with_stack(int (*fn)(int, char **), int argc, char **argv, ULONG size)
{
    BYTE sig = AllocSignal(-1);
    BPTR dir, home, in, out;
    struct Process *child;
    if (sig < 0) return 20;
    child_fn = fn; child_argc = argc; child_argv = argv;
    parent_task = FindTask(NULL);
    parent_sig = 1UL << sig;

    home = GetProgramDir() ? DupLock(GetProgramDir()) : 0;     /* PROGDIR: of the child = the program's drawer */
    dir = Lock((CONST_STRPTR)"", ACCESS_READ);     /* lock on the current directory: the child gets (and frees) it */
    in = Input(); out = Output();
    child = CreateNewProcTags(NP_Entry, (ULONG)child_entry, NP_StackSize, size, NP_Name, (ULONG)"Micropolis",
                              NP_CurrentDir, (ULONG)dir, NP_HomeDir, (ULONG)home,
                              NP_Input, in, NP_CloseInput, FALSE,
                              NP_Output, out, NP_CloseOutput, FALSE, TAG_DONE);
    if (!child) { FreeSignal(sig); return 20; }
    Wait(parent_sig);
    app_log("parent: child signalled");
    FreeSignal(sig);
    app_log("parent: signal freed");
    return child_rc;
}
