/***************************************************************************
 *
 *   midimain.c
 *
 *   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
 *
 ***************************************************************************/

#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include "adlib.h"

/***************************************************************************

    internal function prototypes

***************************************************************************/

static void NEAR PASCAL synthNoteOff(MIDIMSG msg);
static void NEAR PASCAL synthNoteOn(MIDIMSG msg);
static void NEAR PASCAL synthPitchBend(MIDIMSG msg);
static void NEAR PASCAL synthControlChange(MIDIMSG msg);
static void NEAR PASCAL synthProgramChange(MIDIMSG msg);

static BYTE NEAR PASCAL FindVoice(BYTE note, BYTE channel);
static BYTE NEAR PASCAL GetNewVoice(BYTE note, BYTE channel);
static void NEAR PASCAL FreeVoice(BYTE voice);


/***************************************************************************

    local data

***************************************************************************/

typedef struct _VOICE {
    BYTE alloc;               /* is voice allocated? */
    BYTE note;                /* note that is currently being played */
    BYTE channel;             /* channel that it is being played on */
    BYTE volume;              /* current volume setting of voice */
    DWORD dwTimeStamp;        /* time voice was allocated */
} VOICE;

static VOICE voices[11];      /* 9 voices if melodic mode or 11 if percussive */

typedef struct _CHANNEL {
    BYTE patch;               /* the patch on this channel */
    WORD wPitchBend;
} CHANNEL;

/* which patch and PB value (0x2000 = normal) is active on which channel */
static CHANNEL channels[NUMCHANNELS] = {
    0, 0x2000,    /* 0 */
    0, 0x2000,    /* 1 */
    0, 0x2000,    /* 2 */
    0, 0x2000,    /* 3 */
    0, 0x2000,    /* 4 */
    0, 0x2000,    /* 5 */
    0, 0x2000,    /* 6 */
    0, 0x2000,    /* 7 */
    0, 0x2000,    /* 9 */
    0, 0x2000,    /* 8 */
    0, 0x2000,    /* 10 */
    0, 0x2000,    /* 11 */
    0, 0x2000,    /* 12 */
    0, 0x2000,    /* 13 */
    0, 0x2000,    /* 14 */
    129, 0x2000   /* 15 - percussive channel */
};

static BYTE loudervol[128] = {
    0,   0,  65,  65,  66,  66,  67,  67,         /* 0 - 7 */
   68,  68,  69,  69,  70,  70,  71,  71,         /* 8 - 15 */
   72,  72,  73,  73,  74,  74,  75,  75,         /* 16 - 23 */
   76,  76,  77,  77,  78,  78,  79,  79,         /* 24 - 31 */
   80,  80,  81,  81,  82,  82,  83,  83,         /* 32 - 39 */
   84,  84,  85,  85,  86,  86,  87,  87,         /* 40 - 47 */
   88,  88,  89,  89,  90,  90,  91,  91,         /* 48 - 55 */
   92,  92,  93,  93,  94,  94,  95,  95,         /* 56 - 63 */
   96,  96,  97,  97,  98,  98,  99,  99,         /* 64 - 71 */
  100, 100, 101, 101, 102, 102, 103, 103,         /* 72 - 79 */
  104, 104, 105, 105, 106, 106, 107, 107,         /* 80 - 87 */
  108, 108, 109, 109, 110, 110, 111, 111,         /* 88 - 95 */
  112, 112, 113, 113, 114, 114, 115, 115,         /* 96 - 103 */
  116, 116, 117, 117, 118, 118, 119, 119,         /* 104 - 111 */
  120, 120, 121, 121, 122, 122, 123, 123,         /* 112 - 119 */
  124, 124, 125, 125, 126, 126, 127, 127};        /* 120 - 127 */


static char patchKeyOffset[] = {
       0, -12,  12,   0,   0,  12, -12,   0,   0, -24,   /* 0 - 9 */
       0,   0,   0,   0,   0,   0,   0,   0, -12,   0,   /* 10 - 19 */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   /* 20 - 29 */
       0,   0,  12,  12,  12,   0,   0,  12,  12,   0,   /* 30 - 39 */
     -12, -12,   0,  12, -12, -12,   0,  12,   0,   0,   /* 40 - 49 */
     -12,   0,   0,   0,  12,  12,   0,   0,  12,   0,   /* 50 - 59 */
       0,   0,  12,   0,   0,   0,  12,  12,   0,  12,   /* 60 - 69 */
       0,   0, -12,   0, -12, -12,   0,   0, -12, -12,   /* 70 - 79 */
       0,   0,   0,   0,   0, -12, -19,   0,   0, -12,   /* 80 - 89 */
       0,   0,   0,   0,   0,   0, -31, -12,   0,  12,   /* 90 - 99 */
      12,  12,  12,   0,  12,   0,  12,   0,   0,   0,   /* 100 - 109 */
       0,  12,   0,   0,   0,   0,  12,  12,  12,   0,   /* 110 - 119 */
       0,   0,   0,   0, -24, -36,   0,   0};            /* 120 - 127 */


static DWORD dwAge;           /* voice relative age */

#define msg_ch         msg.ch /* all messages */

#define msg_note       msg.b1 /* noteoff(0x80),noteon(0x90),keypressure(0xA0) */
#define msg_controller msg.b1 /* controlchange(0xB0) */
#define msg_patch      msg.b1 /* programchange(0xC0) */
#define msg_cpress     msg.b1 /* channelpressure(0xD0) */
#define msg_lsb        msg.b1 /* pitchbend(0xE0) */

#define msg_velocity   msg.b2 /* noteoff(0x80), noteon(0x90) */
#define msg_kpress     msg.b2 /* keypressure(0xA0) */
#define msg_value      msg.b2 /* controlchange(0xB0) */
#define msg_unused     msg.b2 /* programchange(0xC0), channelpressure(0xD0) */
#define msg_msb        msg.b2 /* pitchbend(0xE0) */

/***************************************************************************
 *
 *  MIDI function director array
 *
 *  (x = channel)
 *  0x8x        Note Off
 *  0x9x        Note On                (vel 0 == Note Off)
 *  0xAx        Key Pressure (Aftertouch)
 *  0xBx        Control Change
 *  0xCx        Program Change
 *  0xDx        Channel Pressure (Aftertouch)
 *  0xEx        Pitch Bend Change
 *  0xF0        Sysex
 *  011111sssb  System Common
 *  011110tttb  System Real Time
 *
 *************************************************************************/
void (NEAR PASCAL * synthmidi [8]) (MIDIMSG);

    void (NEAR PASCAL * synthmidi []) () = {
        synthNoteOff,
        synthNoteOn,
        NULL,                   /* key pressure not currently implemented */
        synthControlChange,
        synthProgramChange,
        NULL,                   /* channel pressure not currently implemented */
        synthPitchBend,
        NULL                    /* sysex etc. not currently implemented */
    };

/***************************************************************************

    public functions

***************************************************************************/

/**************************************************************************
 * @doc INTERNAL
 *
 * @func void | synthMidiData | Process a stream of MIDI messages by calling
 *     the appropriate function based on each status byte.  The function deals
 *     with messages spanning buffers, and with invalid data being passed.
 *
 *     In general, the function steps through the buffer, using the current
 *     state in determining what to look for in checking the next data byte.
 *     This may mean looking for a new status byte, or looking for data
 *     associated with a current status whose data has not been completely
 *     read yet.
 *
 *     The key item to determine the current state of message processing is
 *     the "bCurrentLen" global static byte, which indicates the number of
 *     bytes that are needed to complete the current message being processed.
 *     The current message is stored in the local static "bCurrentStatus", not
 *     to be confused with the global static "status", which is the running
 *     status.  The local static "msg" is used to build the current message,
 *     and is static in order to enable messages to cross buffer boundaries.
 *     The local static "bPosition" determines where in the message buffer to
 *     place the next byte of the message, if any.
 *
 *     The first item in the processing loop is a check for the presence of a
 *     real time message, which can occur anywhere in the data stream, and does
 *     not affect the current state (unless it is possibly a reset command).
 *     Real time messages are ignored for now, including the reset command.
 *     After ignoring them, the loop is continued in case that was the last
 *     byte in the buffer.  If the loop was not continued at this point, the
 *     message sending portion would not function correctly, as "bCurrentLen"
 *     and "bCurrentStatus" is not modified by a real time message.
 *
 *     The next loop item checks to determine if a message is currently being
 *     built.  If "bCurrentLen" is zero, no message is being built, and the
 *     next status byte can be retrieved.  At this point, the current message
 *     position is reset, as any new byte will be the first for this new
 *     message.
 *
 *     If the next byte is a system command, as opposed to a channel command,
 *     it must reset the current running status, and extract the message length
 *     from a different message length table (subtracting one for the status
 *     already retrieved).  Even though these messages are eventually ignored,
 *     the actual message buffer is built as normal.  This will enable a
 *     function to be attached to the message function table which would deal
 *     with system messages.  Note that the system message id is placed into
 *     the channel portion of the message, in place of the channel for a normal
 *     message.
 *
 *     If the next byte is not a system command, then it might be either a
 *     new status byte, or a data byte which depends upon the running status.
 *     If it is a new status byte, running status is updated.  If it is not,
 *     and there is not running status, the byte is ignored, and the loop is
 *     continued.  This might be the case when ignoring data after a SYSEX,
 *     or when invalid data occurs.  If a valid status byte is retrieved, or
 *     is already present in the running status, the internal current status
 *     is updated, in case this message spans buffers, and the length of the
 *     message is retrieved from the channel table (subtracting one for the
 *     status byte already retrieved).
 *
 *     At this point, the message may be completely built (i.e., a one byte
 *     message), in which case, it will fall into the message dispatch code.
 *
 *     The next loop item is fallen into if a message is currently being
 *     processed.  It checks the next byte in the buffer to ensure that it
 *     is actually a data byte.  If it is a status byte instead, the current
 *     message is aborted by resetting "bCurrentLen", and the loop is
 *     continued.  Note that running status is not reset in this case.
 *
 *     If however the next byte is valid, it is placed in either the first or
 *     the second data position in the message being built.  The local static
 *     "bPosition" is used to determine which in position to place the data.
 *
 *     The next loop item checks to see if a complete message has been built.
 *     If so, it dispatches the message based on the current command.  It does
 *     not use running status, as that might have been reset for a system
 *     command.  If a function for the particular command is present, the
 *     message is dispatched, else it is ignored.  If the message was not
 *     complete, the next pass through the loop will pick up the next data
 *     byte for the message.
 *
 *     The loop then continues until it is out of data.
 *
 * @parm HPBYTE | lpBuf | Points to a buffer containing the stream of MIDI
 *     data.
 *
 * @parm DWORD | dwLength | Contains the length of the data pointed to by
 *     <p>lpBuf<d>.
 *
 * @rdesc There is no return value.
*************************************************************************/
void NEAR PASCAL synthMidiData(HPBYTE lpBuf, DWORD dwLength)
{
static MIDIMSG msg;
static BYTE bCurrentStatus;
static BYTE bPosition;
BYTE bByte;

    for (; dwLength; dwLength--) {
        bByte = *lpBuf++;
        
        if (bByte >= STATUS_TIMINGCLOCK)
            continue;

        if (!bCurrentLen) {
kludge_city:
            bPosition = 0;
            if (bByte >= STATUS_SYSEX) {
                bCurrentStatus = bByte;
                status = 0;
                bCurrentLen = (BYTE)(SYSLENGTH(bCurrentStatus) - 1);
            }
	    else {
                if (bByte >= STATUS_NOTEOFF)
                    status = bByte;
                else if (!status)
                    continue;
                bCurrentStatus = status;
                bCurrentLen = (BYTE)(MIDILENGTH(status) - 1);
                if (bByte < STATUS_NOTEOFF)
                    goto first_byte;
            }
            msg_ch = FILTERSTATUS(bCurrentStatus);
        }
	else {
            if (bByte >= STATUS_NOTEOFF)
                goto kludge_city;
            if (!bPosition) {
first_byte:
                bPosition++;
                msg.b1 = bByte;
            }
	    else
                msg.b2 = bByte;
            bCurrentLen--;
        }
        if (!bCurrentLen) {
            bByte = (BYTE)((bCurrentStatus >> 4) & 0x07);
            if (*synthmidi[bByte])
                (*synthmidi[bByte]) (msg);
            else
                D1("MIDI message type not supported");
        }
    }
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthAllNotesOff | Turn any notes off which are playing.
 *
 * @rdesc There is no return value.
 ************************************************************************/
void NEAR PASCAL synthAllNotesOff(void)
{
BYTE voice;
MIDIMSG msg;
    
    for (voice = 0; voice < (BYTE)NUMVOICES; voice++) {
        if (voices[voice].alloc) {
            msg_ch = voices[voice].channel;
            msg_note = voices[voice].note;
            synthNoteOff(msg);
        }
    }
}

/***************************************************************************

    private functions

***************************************************************************/

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthOctaveReg | Perform octave registration on a message.
 *
 * @parm BYTE | msg_ch | The channel the note is to be played on.
 *
 * @parm BYTE | msg_note | The MIDI note number.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthOctaveReg(MIDIMSG FAR *pMsg)
{
int  iModNote;
MIDIMSG msg = *pMsg;

    if ((msg_ch == DRUMKITCHANNEL) || (channels[msg_ch].patch > 127))
        return;  /* only affect normal melodic patches */
    
    iModNote = msg_note + patchKeyOffset[channels[msg_ch].patch];
    if ((iModNote < 0) || (iModNote > 127))
        iModNote = msg_note;

    msg_note = (BYTE) iModNote;
    *pMsg = msg;  /* modify what was pointed to */
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthNoteOn | Turn on a requested note.
 *
 * @parm BYTE | msg_ch | The channel the note is to be played on.
 *
 * @parm BYTE | msg_note | The MIDI note number.
 *
 * @parm BYTE | msg_velocity | The velocity level for the note.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthNoteOn(MIDIMSG msg)
{
BYTE voice;

    if (msg_velocity == 0) {               /* 0 velocity means note off */
        synthNoteOff(msg);
        return;
    }

    synthOctaveReg(&msg);  /* adjust key # to overcome patch octave diffs */

    /* hack to overcome how quiet this thing is in relation to wave output */
    msg_velocity = loudervol[msg_velocity];

    if (msg_ch == DRUMKITCHANNEL) {       /* drum kit hardwired on channel 15 */
        if ((msg_note < FIRSTDRUMNOTE) || (msg_note > LASTDRUMNOTE))
            return;

        channels[DRUMKITCHANNEL].patch = drumpatch[msg_note - FIRSTDRUMNOTE].patch;
        msg_note = drumpatch[msg_note - FIRSTDRUMNOTE].note;

        if ((voice = FindVoice(msg_note, msg_ch)) != 0xFF)
            NoteOff(voice);
        voice = GetNewVoice(msg_note, msg_ch);
    }

    else {
        voice = FindVoice(msg_note, msg_ch);       /* voice already assigned? */
        if (voice == 0xff)
            voice = GetNewVoice(msg_note, msg_ch); /* if not, get one */
        else
            NoteOff(voice);
    }

    if (voices[voice].volume != msg_velocity) { /* check if it's already set */
        SetVoiceVolume(voice, msg_velocity);
        voices[voice].volume = msg_velocity;
    }
    /* apply any pb for this channel */
    SetVoicePitch(voice, channels[msg_ch].wPitchBend);
    /* adjust for octave reg. */
    NoteOn(voice, msg_note);
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthNoteOff | Turn off a requested note.
 *
 * @parm BYTE | msg_ch | The channel the note is on.
 *
 * @parm BYTE | msg_note | The MIDI note number.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthNoteOff(MIDIMSG msg)
{
BYTE voice;
    
    /* adjust key # to overcome patch octave differences */
    synthOctaveReg(&msg); 

    if (msg_ch == DRUMKITCHANNEL) {       /* drum kit hardwired on channel 15 */
        BYTE bPatch;

        if ((msg_note < FIRSTDRUMNOTE) || (msg_note > LASTDRUMNOTE))
            return;

        bPatch = drumpatch[msg_note - FIRSTDRUMNOTE].patch;
        msg_note = drumpatch[msg_note - FIRSTDRUMNOTE].note;

        if ((voice = FindVoice(msg_note, msg_ch)) != 0xFF) {
            if (LOWORD(voices[voice].dwTimeStamp) == bPatch) {
                NoteOff(voice);
                FreeVoice(voice);
            }
        }
        return;
    }

    else {
        voice = FindVoice(msg_note, msg_ch);       /* get the assigned voice */
    }

    if (voice == 0xFF)
        return;

    /* turn the note off */

    if (voices[voice].note) {               /* check if note is playing */
        NoteOff(voice);
        /* return note to pool of notes. */
        FreeVoice(voice);
    }
}

#if 0
/* These functions are commented out because we are not currently supporting
 * channel and key pressure messages in this driver.  Ad Lib had originally
 * interpreted them as volume values, which produces incorrect results.
 * I haven't implemented them because it's not clear from the technical
 * documentation how produce the low-frequency oscillation that is often
 * produced by these messages.  To support the messages, change the
 * entries in the synthmidi array to call these functions again, uncomment
 * these two functions, and define an xSetVoicePressure routine.
 */

static void NEAR PASCAL synthKeyPressure(MIDIMSG msg);
static void NEAR PASCAL synthChannelPressure(MIDIMSG msg);
/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthChannelPressure | Set the pressure for a channel.
 *
 * @parm BYTE | msg_ch | The channel to be set.
 *
 * @parm BYTE | msg_cpress | The pressure level for the channel.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthChannelPressure(MIDIMSG msg)
{
int i;

    for (i = 0; i < NUMVOICES; i++) {
        if ((voices[i].alloc) && (voices[i].channel == msg_ch))
            xSetVoicePressure(i, msg_cpress);
    }
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthKeyPressure | Set the key pressure for a note.
 *
 * @parm BYTE | msg_ch | The channel the note is on.
 *
 * @parm BYTE | msg_note | The MIDI note number.
 *
 * @parm BYTE | msg_kpress | The pressure level for the note.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthKeyPressure(MIDIMSG msg)
{
BYTE voice;

    voice = FindVoice(msg_note, msg_ch);
    if (voice == 0xFF)
        return;

    xSetVoicePressure(voice, msg_kpress);
}
#endif

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthPitchBend | Bend the pitch.
 *
 * @parm BYTE | msg_ch | The channel to bend the pitch for.
 *
 * @parm BYTE | msg_lsb | LSB of pitch bend.
 *
 * @parm BYTE | msg_msb | MSB of pitch bend.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthPitchBend(MIDIMSG msg)
{
BYTE i;
WORD wPB = (((WORD)msg_msb) << 7) | msg_lsb;

    /* msb is shifted by 7 because we've redefined the MIDI pitch bend
     * range of 0 - 0x7f7f to 0 - 3fff by concatenating the two
     * 7-bit values in msb and lsb together
     */

    for (i = 0; i < (BYTE)NUMVOICES; i++) {
        if ((voices[i].alloc) && (voices[i].channel == msg_ch))
            SetVoicePitch(i, wPB);
    }
    channels[msg_ch].wPitchBend = wPB; /* remember for subsequent notes */
                                       /* on this channel */
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api void | synthControlChange | Change a controller value.
 *
 * @parm BYTE | msg_ch | The MIDI channel.
 *
 * @parm BYTE | msg_controller | The controller number to change.
 *
 * @parm BYTE | msg_value | The value to change the controller to.
 *
 * @comm The only controllers supported are 123-127 (all notes off).
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthControlChange(MIDIMSG msg)
{
    if (msg_controller >= 123)
        synthAllNotesOff();
}

/**************************************************************************
 *  @doc INTERNAL
 *
 *  @api void | synthProgramChange | Change a channel patch assignment.
 *
 *  @parm BYTE | msg_ch | The channel the patch is to apply to.
 *
 *  @parm BYTE | msg_patch | The new patch number.
 *
 *  @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL synthProgramChange(MIDIMSG msg)
{
BYTE voice;

    /* drum kit hardwired on channel 15, so ignore patch changes there */
    if (msg_ch == DRUMKITCHANNEL)
        return;

    /* turn off any notes on this channel which are currently on */
    for (voice = 0; voice < (BYTE)NUMVOICES; voice++) {
        if ((voices[voice].alloc) && (voices[voice].channel == msg_ch)
             && (voices[voice].note)) {      /* check if note is playing */
                NoteOff(voice);              /* turn note off */
                FreeVoice(voice);            /* return note to pool of notes. */
        }
    }

    /* change the patch for this channel */
    channels[msg_ch].patch = msg_patch;

}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api BYTE | FindVoice | Find the voice a note/channel is using.
 *
 * @parm BYTE | note | The note in use.
 *
 * @parm BYTE | channel | The channel in use.
 *
 * @rdesc There return value is the voice number or 0xFF if no match is found.
 **************************************************************************/
static BYTE NEAR PASCAL FindVoice(BYTE note, BYTE channel)
{
BYTE i;

    if (channel == DRUMKITCHANNEL) {
        i = patches[channels[DRUMKITCHANNEL].patch].percVoice;

        if (voices[i].alloc)
            return i;
    }

    else {
        for (i = 0; i < (BYTE)NUMVOICES; i++) {
            if ((voices[i].alloc) && (voices[i].note == note)
            && (voices[i].channel == channel)) {
                voices[i].dwTimeStamp = dwAge++;
                return i;
            }
        }
    }

    return 0xFF;                          /* not found */
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api BYTE | GetNewVoice | Find a new voice to play a note.  Uses an LRU
 *     algorithm to steal voices.  The timestamp is set at allocation time
 *     incremented in <f FindVoice>.
 *
 * @parm BYTE | note | The note we want to play.
 *
 * @parm BYTE | channel | The channel we want to play it on.
 *
 * @rdesc Returns the voice number.
 **************************************************************************/
static BYTE NEAR PASCAL GetNewVoice(BYTE note, BYTE channel) 
{
BYTE  i;
BYTE  voice;
BYTE  patch;
BYTE  bVoiceToUse;
DWORD dwOldestTime = dwAge;                     /* init to current "time" */

    /* get the patch in use for this channel */
    patch = channels[channel].patch;

    if (patches[patch].mode) {                  /* it's a percussive patch */
        voice = patches[patch].percVoice;       /* use fixed percussion voice */
        voices[voice].alloc = TRUE;
        voices[voice].note = note;
        voices[voice].channel = channel;
        voices[voice].dwTimeStamp = MAKELONG(patch, 0);
        SetVoiceTimbre(voice, &patches[patch].op0);  /* set the timbre */
        return voice;
    }

    /* find a free melodic voice to use */
    for (i = 0; i < (BYTE)NUMMELODIC; i++) {  /* it's a melodic patch */
        if (!voices[i].alloc) {
            bVoiceToUse = i;                  /* grab first unused one */
            break;
        }
        else if (voices[i].dwTimeStamp < dwOldestTime) {
                dwOldestTime = voices[i].dwTimeStamp;
                bVoiceToUse = i;              /* remember oldest one to steal */
        }
    }

    /* at this point, we have either found an unused voice, */
    /* or have found the oldest one among a totally used voice bank */

    if (voices[bVoiceToUse].alloc)            /* if we stole it, turn it off */
        NoteOff(bVoiceToUse);

    voices[bVoiceToUse].alloc = 1;
    voices[bVoiceToUse].note = note;
    voices[bVoiceToUse].channel = channel;
    voices[bVoiceToUse].dwTimeStamp = dwAge++;

    SetVoiceTimbre(bVoiceToUse, &patches[patch].op0);   /* set the timbre */

    return bVoiceToUse;
}

/**************************************************************************
 * @doc INTERNAL
 *
 * @api BYTE | FreeVoice | Free a voice we have been using.
 *
 * @parm BYTE | voice | The voice to free.
 *
 * @rdesc There is no return value.
 **************************************************************************/
static void NEAR PASCAL FreeVoice(BYTE voice) 
{
    voices[voice].alloc = 0;
}
