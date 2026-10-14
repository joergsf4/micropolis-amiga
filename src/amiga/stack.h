#ifndef STACK_H
#define STACK_H
#include <exec/types.h>
int run_with_stack(int (*fn)(int, char **), int argc, char **argv, ULONG size);
#define BIG_STACK (256 * 1024)
#endif
