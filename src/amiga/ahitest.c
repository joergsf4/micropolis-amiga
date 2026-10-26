/* Checks AHI: opens ahi.device (unit 0 = the user's default) the textbook way and plays a 440 Hz tone for 2 seconds as
 * 8 bit signed mono at 11025 Hz, the format of the game's sounds. usage: ahitest [unit] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
    ULONG unit = argc > 1 ? (ULONG)atoi(argv[1]) : AHI_DEFAULT_UNIT;
    struct MsgPort *port = CreateMsgPort();
    struct AHIRequest *req = (struct AHIRequest *)CreateIORequest(port, sizeof(struct AHIRequest));
    int n = 11025 * 2, i;
    BYTE *buf = AllocVec(n, MEMF_PUBLIC);
    BYTE err;
    if (!port || !req || !buf) { printf("no memory\n"); return 20; }
    for (i = 0; i < n; i++) { int ph = (i * 440 / 11025) ; (void)ph; buf[i] = ((i * 440 * 2 / 11025) & 1) ? 90 : -90; }     /* square wave of 440 Hz */
    req->ahir_Version = 4;
    err = OpenDevice((CONST_STRPTR)"ahi.device", unit, (struct IORequest *)req, 0);
    if (err) { printf("OpenDevice ahi.device unit %lu failed: %d\n", (unsigned long)unit, (int)err); return 10; }
    printf("ahi.device open (version %d.%d)\n", (int)req->ahir_Std.io_Device->dd_Library.lib_Version, (int)req->ahir_Std.io_Device->dd_Library.lib_Revision);
    req->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
    req->ahir_Std.io_Command = CMD_WRITE;
    req->ahir_Std.io_Data = buf;
    req->ahir_Std.io_Length = n;
    req->ahir_Std.io_Offset = 0;
    req->ahir_Type = AHIST_M8S;
    req->ahir_Frequency = 11025;
    req->ahir_Volume = 0x10000;
    req->ahir_Position = 0x8000;
    req->ahir_Link = NULL;
    DoIO((struct IORequest *)req);
    printf("played, io_Error %d\n", (int)req->ahir_Std.io_Error);
    CloseDevice((struct IORequest *)req);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    FreeVec(buf);
    return 0;
}
