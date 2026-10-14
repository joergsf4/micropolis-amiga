/* The upstream core keeps large buffers on the stack; the shell/Workbench stack is only 4 KB, and
 * libnix' __stack variable is not honoured by this toolchain. So main() runs on its own stack. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include "stack.h"

int run_with_stack(int (*fn)(int, char **), int argc, char **argv, ULONG size)
{
    UBYTE *mem = AllocMem(size, MEMF_ANY);
    struct StackSwapStruct sss;
    int rc;
    if (!mem) return 20;
    sss.stk_Lower = mem;
    sss.stk_Upper = (ULONG)(mem + size);
    sss.stk_Pointer = (APTR)sss.stk_Upper;
    StackSwap(&sss);
    rc = fn(argc, argv);
    StackSwap(&sss);
    FreeMem(mem, size);
    return rc;
}
