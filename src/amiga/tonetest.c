/* Plays a 1 kHz tone for 2 seconds through audio.device the textbook way (RKM "Simple_Sound"), on one channel.
 * Used to check whether a test setup can record Paula output at all. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/audio.h>
#include <proto/exec.h>

int main(void)
{
    static UBYTE chan[] = { 1, 2, 4, 8 };
    struct MsgPort *port = CreateMsgPort();
    struct IOAudio *ioa = (struct IOAudio *)CreateIORequest(port, sizeof(struct IOAudio));
    BYTE *wave = AllocMem(64, MEMF_CHIP);
    int err;
    if (!port || !ioa || !wave) return 20;
    for (int i = 0; i < 64; i++) wave[i] = (i < 32) ? 100 : -100;          /* square wave, 64 samples per cycle */
    ioa->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
    ioa->ioa_Data = chan;
    ioa->ioa_Length = sizeof chan;
    err = OpenDevice((CONST_STRPTR)"audio.device", 0, (struct IORequest *)ioa, 0);
    printf("OpenDevice: %d\n", err);
    if (err) return 10;
    ioa->ioa_Request.io_Command = CMD_WRITE;
    ioa->ioa_Request.io_Flags = ADIOF_PERVOL;
    ioa->ioa_Data = (UBYTE *)wave;
    ioa->ioa_Length = 64;
    ioa->ioa_Period = 124;                 /* 3546895 / 124 / 64 = 447 Hz */
    ioa->ioa_Volume = 64;
    ioa->ioa_Cycles = 900;                 /* about 2 seconds */
    SendIO((struct IORequest *)ioa);
    WaitIO((struct IORequest *)ioa);
    printf("done, error %d\n", ioa->ioa_Request.io_Error);
    CloseDevice((struct IORequest *)ioa);
    DeleteIORequest((struct IORequest *)ioa);
    DeleteMsgPort(port);
    FreeMem(wave, 64);
    return 0;
}
