/*****************************************************************************
 *
 *   midic.c
 *
 *   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
 *
 ***************************************************************************/

#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include "adlib.h"

/*****************************************************************************

    internal function prototypes

*****************************************************************************/ 

static void NEAR PASCAL midiCallback(NPSYNTHALLOC pClient, WORD msg, DWORD dw1, DWORD dw2);

static void FAR PASCAL GetSynthCaps(LPBYTE lpCaps, WORD wSize);
#pragma alloc_text(_TEXT, GetSynthCaps)

/***************************************************************************

    local data

***************************************************************************/

static SYNTHALLOC gClient;     /* client information */
static WORD wAllocated;        /* have we already been allocated? */
static int synthreenter;       /* reentrancy check */
BYTE status;                   /* don't make assumptions about initial status */
BYTE bCurrentLen;


#define FIXED_DS()  HIWORD((DWORD)(LPVOID)(&wAllocated))
#define FIXED_CS()  HIWORD((DWORD)(LPVOID)modMessage)

BYTE gbMidiLengths[] =
{
        3,      /* STATUS_NOTEOFF */
        3,      /* STATUS_NOTEON */
        3,      /* STATUS_POLYPHONICKEY */
        3,      /* STATUS_CONTROLCHANGE */
        2,      /* STATUS_PROGRAMCHANGE */
        2,      /* STATUS_CHANNELPRESSURE */
        3,      /* STATUS_PITCHBEND */
};

BYTE gbSysLengths[] =
{
        1,      /* STATUS_SYSEX */
        2,      /* STATUS_QFRAME */
        3,      /* STATUS_SONGPOINTER */
        2,      /* STATUS_SONGSELECT */
        1,      /* STATUS_F4 */
        1,      /* STATUS_F5 */
        1,      /* STATUS_TUNEREQUEST */
        1,      /* STATUS_EOX */
};

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | midiCallback | This calls DriverCallback, which calls the
 *     client's callback or window if the client has requested notification.
 *
 * @parm NPSYNTHALLOC | pClient | Pointer to the SYNTHALLOC structure.
 *
 * @parm WORD | msg | The message to send.
 *
 * @parm DWORD | dw1 | Message-dependent parameter.
 *
 * @parm DWORD | dw2 | Message-dependent parameter.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL midiCallback(NPSYNTHALLOC pClient, WORD msg, DWORD dw1, DWORD dw2)
{
    /* dwFlags contains midi driver specific flags in the LOWORD */
    /* and generic driver flags in the HIWORD */

    if (pClient->dwCallback)
        DriverCallback(pClient->dwCallback,      /* client's callback DWORD */
                       HIWORD(pClient->dwFlags), /* callback flags */
                       pClient->hMidiOut,        /* handle to the wave device */
                       msg,                      /* the message */
                       pClient->dwInstance,      /* client's instance data */
                       dw1,                      /* first DWORD */
                       dw2);                     /* second DWORD */
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | GetSynthCaps | Get the capabilities of the synth.
 *
 * @parm LPBYTE | lpCaps | Far pointer to a MIDICAPS structure.
 *
 * @parm WORD | wSize | Size of the MIDICAPS structure.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void FAR PASCAL GetSynthCaps(LPBYTE lpCaps, WORD wSize)
{
MIDIOUTCAPS mc;    /* caps structure we know about */
LPBYTE      mp;    /* place in client's buffer */
WORD        w;     /* number of bytes to copy */

    mc.wMid = MM_MICROSOFT;
    mc.wPid = MM_ADLIB;
    mc.wTechnology = MOD_FMSYNTH;
    mc.wVoices = NUMVOICES;
    mc.wNotes = NUMVOICES;
    mc.wChannelMask = 0xff;                       /* all channels */
    mc.vDriverVersion = DRIVER_VERSION;
    mc.dwSupport = 0L;
    lstrcpy(mc.szPname, aszProductName);

    /* copy as much as will fit into client's buffer */
    w = min(wSize, sizeof(MIDIOUTCAPS));
    mp = (LPBYTE)&mc;
    while (w--) *lpCaps++ = *mp++;
}
/****************************************************************************

    This function conforms to the standard MIDI output driver message proc
    modMessage, which is documented in the multimedia DDK.

 ***************************************************************************/
DWORD FAR PASCAL _loadds modMessage(WORD id, WORD msg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2)
{
LPMIDIHDR    lpHdr;        /* header of long message buffer */
LPSTR lpBuf;               /* current spot in long message buffer */
DWORD dwLength;            /* length of data being processed */
    
    /* has the whole card been enabled? */
    if (!fEnabled) {
        D1("modMessage called while disabled");
        if (msg == MODM_GETNUMDEVS)
            return 0L;
        else
            return MMSYSERR_NOTENABLED;
    }

    /* this driver only supports one device */
    if (id != 0) {               
        D1("invalid midi device id");
        return MMSYSERR_BADDEVICEID;
    }

    switch (msg) {
        
        case MODM_GETNUMDEVS:   
            D1("MODM_GETNUMDEVS");
            return 1L;

        case MODM_GETDEVCAPS:
            D1("MODM_GETDEVCAPS");
            GetSynthCaps((LPBYTE)dwParam1, (WORD)dwParam2);
            return 0L;

        case MODM_OPEN:
            D1("MODM_OPEN");

            /* check if allocated */
            if (wAllocated)
                return MMSYSERR_ALLOCATED;

            if (vadlibdAcquireAdLibSynth()) {
                D1("AdLib could NOT be aquired!!!");
                return MMSYSERR_ALLOCATED;
            }
            
            {
                extern void FAR SoundWarmInit(void);
                SoundWarmInit();
            }

            /* save client information */
            gClient.dwCallback = ((LPMIDIOPENDESC)dwParam1)->dwCallback;
            gClient.dwInstance = ((LPMIDIOPENDESC)dwParam1)->dwInstance;
            gClient.hMidiOut   = ((LPMIDIOPENDESC)dwParam1)->hMidi;
            gClient.dwFlags    = dwParam2;

            /* !!! fix for 3.0 286p mode */
            if (!GlobalPageLock(FIXED_DS()) || !GlobalPageLock(FIXED_CS())) {
                vadlibdReleaseAdLibSynth();
                return MMSYSERR_NOMEM;
            }

            wAllocated++;
            bCurrentLen = 0;
            status = 0;

            /* notify client */
            midiCallback(&gClient, MOM_OPEN, 0L,  0L);

            return 0L;
            
        case MODM_CLOSE:
            D1("MODM_CLOSE");

            /* shut up */
            synthAllNotesOff();

            midiCallback(&gClient, MOM_CLOSE, 0L,  0L);

            /* get out */
            wAllocated--;

            GlobalPageUnlock(FIXED_DS());
            GlobalPageUnlock(FIXED_CS());

            if (vadlibdReleaseAdLibSynth())
                D1("AdLib could NOT be RELEASED!!! VERY GOOFY!!");

            return 0L;
            
        case MODM_RESET:
            D1("MODM_RESET");

            /* we don't need to return all long buffers since we've */
            /* implemented MODM_LONGDATA synchronously */
            synthAllNotesOff();
            return 0L;

        case MODM_DATA:
            D4("MODM_DATA");

            /* make sure we're not being reentered */
            synthreenter++;
            if (synthreenter > 1) {
                D1("MODM_DATA reentered!");
                synthreenter--;
                return MIDIERR_NOTREADY;
            }

            lpBuf = (LPBYTE)&dwParam1;
            if (*lpBuf >= STATUS_TIMINGCLOCK)
                dwLength = 1;
            else {
                bCurrentLen = 0;
                if (ISSTATUS(*lpBuf)) {
                    if (*lpBuf >= STATUS_SYSEX)
                        dwLength = SYSLENGTH(*lpBuf);
                    else
                        dwLength = MIDILENGTH(*lpBuf);
                }
		else {
                    if (!status)
                        return 0L;
                    dwLength = MIDILENGTH(status) - 1;
                }
            }
            synthMidiData(lpBuf, dwLength);

            synthreenter--;
            return 0L;

        case MODM_LONGDATA:
            D1("MODM_LONGDATA");

            /* make sure we're not being reentered */
            synthreenter++;
            if (synthreenter > 1) {
                D1("MODM_LONGDATA reentered!");
                synthreenter--;
                return MIDIERR_NOTREADY;
            }

            /* check if it's been prepared */
            lpHdr = (LPMIDIHDR)dwParam1;
            if (!(lpHdr->dwFlags & MHDR_PREPARED)) {
                synthreenter--;
                return MIDIERR_UNPREPARED;
            }

            synthMidiData(lpHdr->lpData, lpHdr->dwBufferLength);

            /* return buffer to client */
            lpHdr->dwFlags |= MHDR_DONE;
            midiCallback(&gClient, MOM_DONE, dwParam1,  0L);

            synthreenter--;
            return 0L;

        default:
            return MMSYSERR_NOTSUPPORTED;
    }

    /* should never get here... */
    return MMSYSERR_NOTSUPPORTED;
}
