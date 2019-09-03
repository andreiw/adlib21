/****************************************************************************
 *
 *   adlib.c
 *
 *   Copyright Ad Lib Inc, 1988, 1989
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

static void NEAR PASCAL ChangePitch(BYTE voice, WORD pitchBend);
static void NEAR PASCAL SndSetAllPrm(BYTE slot);
static void NEAR PASCAL SndSAVEK(BYTE slot);
static void NEAR PASCAL SndSFeedFm(BYTE slot);
static void NEAR PASCAL SndSAttDecay(BYTE slot);
static void NEAR PASCAL SndSSusRelease(BYTE slot);
static void NEAR PASCAL SndWaveSelect(BYTE slot);

/***************************************************************************

    public data

***************************************************************************/

BYTE slotRelVolume[18];     /* relative volume of slots */
BYTE percBits;              /* control bits of percussive voices */
BYTE amDepth;               /* chip global parameters ... */
BYTE vibDepth;              /* ... */
BYTE noteSel;               /* ... */
BYTE modeWaveSel;           /* != 0 if used with 'wave-select' parms */
BOOL fPercussion;           /* percussion mode parameter */
int  pitchRangeStep;        /* == pitchRange * NR_STEP_PITCH */
WORD fNumNotes[NR_STEP_PITCH][12];
NPWORD fNumFreqPtr[11];     /* lines of fNumNotes table (one per voice) */
int  halfToneOffset[11];    /* one per voice */
BYTE noteDIV12[96];         /* table of (0..95) DIV 12 */
BYTE noteMOD12[96];         /* table of (0..95) MOD 12 */

/* this table gives the offset of each slot within the chip. */
BYTE offsetSlot[] = {
         0,  1,  2,  3,  4,  5,
         8,  9, 10, 11, 12, 13,
        16, 17, 18, 19, 20, 21
};

/* this table indicates if the slot is a modulator (0) or a carrier (1). */
BYTE operSlot[] = {
        0, 0, 0,           /* 1 2 3 */
        1, 1, 1,           /* 4 5 6 */
        0, 0, 0,           /* 7 8 9 */
        1, 1, 1,           /* 10 11 12 */
        0, 0, 0,           /* 13 14 15 */
        1, 1, 1,           /* 16 17 18 */
};

/***************************************************************************

    local data

***************************************************************************/

static BYTE notePitch[11];    /* pitch value for each voice (implicit 0 init) */
static BYTE voiceKeyOn[11];   /* keyOn bit for each voice (implicit 0 init) */
static BYTE percMasks[] = {
        0x10, 0x08, 0x04, 0x02, 0x01 };

/* voice number associated with each slot (melodic mode only) */
static BYTE voiceSlot[] = {
        0, 1, 2,
        0, 1, 2,
        3, 4, 5,
        3, 4, 5,
        6, 7, 8,
        6, 7, 8,
};

/* slot numbers for percussive voices
 * (0 indicates that there is only one slot)
 */
static BYTE slotPerc[][2] = {
        {12, 15},        /* Bass Drum */
        {16, 0},         /* SD */
        {14, 0},         /* TOM */
        {17, 0},         /* TOP-CYM */
        {13, 0}          /* HH */
};

/* slot numbers as a function of the voice and the operator (melodic only) */
static BYTE slotVoice[][2] = {
        {0, 3},          /* voice 0 */
        {1, 4},          /* 1 */
        {2, 5},          /* 2 */
        {6, 9},          /* 3 */
        {7, 10},         /* 4 */
        {8, 11},         /* 5 */
        {12, 15},        /* 6 */
        {13, 16},        /* 7 */
        {14, 17}         /* 8 */
};

static BYTE paramSlot[18][NUMLOCPARAM];    /* all the parameters of slots... */

/****************************************************************************

    macros

 ***************************************************************************/

#define GetLocPrm(slot, prm)   ((WORD)paramSlot[slot][prm])

/***************************************************************************

    public functions

***************************************************************************/

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetSlotParam | Sets the 14 parameters (13 in <p param>,
 *     1 in <p waveSel>) of slot <p slot>. Updates both the parameter array
 *     and the chip.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @parm NPBYTE | param | Pointer to the new parameter array.
 *
 * @parm BYTE | waveSel | The new waveSel value. 
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void FAR PASCAL SetSlotParam(BYTE slot, NPBYTE param, BYTE waveSel)
{
int    i;
LPBYTE ptr;

    for (i = 0, ptr = &paramSlot[slot][0]; i < NUMLOCPARAM - 1; i++)
        *ptr++ = *param++;
    *ptr = waveSel &= 0x3;
    SndSetAllPrm(slot);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetFreq | Changes the pitch of voices 0 to 8, for melodic or
 *     percussive mode.
 *
 * @parm BYTE | voice | Specifies which voice to set.
 *
 * @parm BYTE | pitch | Specifies the pitch (0 to 95).
 *
 * @parm BYTE | keyOn | Flag specifying whether the key is on or off.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void FAR PASCAL SetFreq(BYTE voice, BYTE pitch, BYTE keyOn)
{
WORD  FNum;
BYTE  t1;
    
    /* remember the keyon and pitch of the voice */
    voiceKeyOn[voice] = keyOn;
    notePitch[voice] = pitch;

    pitch += halfToneOffset[voice];
    if (pitch > 95)
        pitch = 95;

    /* get the FNum for the voice */
    FNum = * (fNumFreqPtr[voice] + noteMOD12[pitch]);

    /* output the FNum, KeyOn and Block values */
    SndOutput((BYTE)(0xA0 | voice), (BYTE)FNum); /* FNum bits 0 - 7 (D0 - D7) */
    t1 = (BYTE)(keyOn ? 32 : 0);                 /* Key On (D5) */
    t1 += (noteDIV12[pitch] << 2);               /* Block (D2 - D4) */
    t1 += (0x3 & (FNum >> 8));                   /* FNum bits 8 - 9 (D0 - D1) */
    SndOutput((BYTE)(0xB0 | voice), t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSAmVibRhythm | Sets the AM Depth, VIB depth and Rhythm values.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void FAR PASCAL SndSAmVibRhythm(void)
{
BYTE t1;

    t1 = (BYTE)(amDepth ? 0x80 : 0);
    t1 |= vibDepth ? 0x40 : 0;
    t1 |= fPercussion ? 0x20 : 0;
    t1 |= percBits;
    SndOutput(0xBD, t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSNoteSel | Sets the NoteSel value.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void FAR PASCAL SndSNoteSel(void)
{
    SndOutput(0x08, (BYTE)(noteSel ? 64 : 0));
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSKslLevel | Sets the KSL and LEVEL values.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL SndSKslLevel(BYTE slot)
{
WORD t1;

    t1 = 63 - (GetLocPrm(slot, prmLevel) & 0x3f);        /* amplitude */
    t1 = slotRelVolume[slot] * t1;
    t1 += t1 + MAXVOLUME;                                /* round off to 0.5 */
    t1 = 63 - t1 / (2 * MAXVOLUME);

    t1 |= GetLocPrm(slot, prmKsl) << 6;
    SndOutput((BYTE)(0x40 | offsetSlot[slot]), (BYTE)t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetVoiceTimbre | This routine sets the parameters of the
 *     voice <p voice>.
 *
 * @parm BYTE | voice | Specifies which voice to set.
 *
 * @parm NPOPERATOR | pOper0 | Pointer to operator 0.
 *
 * @comm In melodic mode, <p voice> varies from 0 to 8.  In percussive mode,
 *     voices 0 to 5 are melodic and 6 to 10 are percussive.
 *
 *     A timbre (melodic or percussive) is defined as follows: the 13 first
 *     parameters of operator 0 (ksl, multi, feedBack, attack, sustain,
 *     eg-type, decay, release, level, am, vib, ksr, fm), followed by the
 *     13 parameters of operator 1 (if a percussive voice, all the parameters
 *     are zero), followed by the wave-select parameter for the operators 0
 *     and 1.
 *
 *     <p pOper0> is actually pointing to the <e op0> element of the
 *     <T TIMBRE> structure, which is defined as follows:
 *
 *     typedef struct {
 *         BYTE      mode;              0 = melodic, 1 = percussive
 *         BYTE      percVoice;         if mode == 1, voice number to be used
 *         OPERATOR  op0;               a 13 byte array of op0 parameters
 *         OPERATOR  op1;               a 13 byte array of op1 parameters
 *         BYTE      wave0;             waveform for operator 0
 *         BYTE      wave1;             waveform for operator 1
 *     } TIMBRE, *NPTIMBRE, FAR *LPTIMBRE;
 *
 *     The old timbre files (*.INS) do not contain the parameters
 *     'wave0' and 'wave1'.  Set these two parameters to zero if
 *     you are using the old file format.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL SetVoiceTimbre(BYTE voice, NPOPERATOR pOper0)
{
NPBYTE prm0;
NPBYTE prm1;
BYTE   wave0;
BYTE   wave1;
NPBYTE wavePtr;

    prm0 = (NPBYTE)pOper0;
    prm1 = prm0 + NUMLOCPARAM - 1;
    wavePtr = prm0 + 2 * (NUMLOCPARAM - 1);
    wave0 = *wavePtr++;
    wave1 = *wavePtr;

    if (!fPercussion || voice < BD) {        /* melodic only */
        D3("Set melodic voice");
            SetSlotParam(slotVoice[voice][0], prm0, wave0);
            SetSlotParam(slotVoice[voice][1], prm1, wave1);
    }
    else if (voice == BD) {                   /* bass drum */
        D3("Set bass drum");
            SetSlotParam(slotPerc[0][0], prm0, wave0);
            SetSlotParam(slotPerc[0][1], prm1, wave1);
    }
    else {                                    /* percussion, 1 slot */
        D3("Set percussion");
            SetSlotParam(slotPerc[voice - BD][0], prm0, wave0);
    }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetVoicePitch | Changes the pitch value of a voice.  Does
 *     not affect the percussive voices, except for the bass drum.  The change
 *     takes place immediately.
 *
 * @parm BYTE | voice | Specifies which voice to set.
 *
 * @parm WORD | pitchBend | Specifies the new pitch bend value (0 to 0x3fff,
 *     where 0x2000 == exact tuning).
 *
 * @comm The variation in pitch is a function of the previous call to
 *     <f SetPitchRange> and the value of <p pitchBend>.  A value of 0 means
 *     -half-tone * pitchRangeStep, 0x2000 means no variation (exact pitch) and
 *      0x3fff means +half-tone * pitchRangeStep.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL SetVoicePitch(BYTE voice, WORD pitchBend)
{
    if (!fPercussion || voice <= BD) {       /* melodic and bass drum voices */
        if (pitchBend > MAX_PITCH)     
                pitchBend = MAX_PITCH;
        ChangePitch(voice, pitchBend);
        SetFreq(voice, notePitch[voice], voiceKeyOn[voice]);
    }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetVoiceVolume | Sets the volume of the voice <p voice> to
 *     <p volume>. The resulting output level is (timbreVolume * volume / 127).
 *     The change takes place immediately.
 *
 * @parm BYTE | voice | Specifies which voice to set.
 *
 * @parm BYTE | volume | Specifies the new volume level (0 to 127).
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL SetVoiceVolume(BYTE voice, BYTE volume)
{
BYTE   slot;
NPBYTE slots;

    if (volume > MAXVOLUME)
            volume = MAXVOLUME;

    if (!fPercussion || voice <= BD) {      /* melodic voice */
        slots = slotVoice[voice];
        slotRelVolume[slots[1]] = volume;
        SndSKslLevel(slots[1]);
        if (!GetLocPrm(slots[0], prmFm)) {
            /* additive synthesis: set volume of first slot too */
            slotRelVolume[slots[0]] = volume;
            SndSKslLevel(slots[0]);
        }
    }
    else {                                  /* percussive voice */
        slot = slotPerc[voice - BD][0];
        slotRelVolume[slot] = volume;
        SndSKslLevel(slot);
    }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | NoteOn | This routine starts a note playing.
 *
 * @parm BYTE | voice | Specifies which voice to use (0 to 8 in melodic mode,
 *     0 to 10 in percussive mode).
 *
 * @parm BYTE | pitch | Specifies the pitch (0 to 127, where 60 == MID_C).  The
 *     card can play between 12 and 107.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL NoteOn(BYTE voice, BYTE pitch)
{
    D3("NoteON");

    /* adjust pitch for chip */
    if (pitch < (MID_C - CHIP_MID_C))
        pitch = 0;
    else
        pitch -= (MID_C - CHIP_MID_C);
        
    if (voice < BD || !fPercussion)         /* this is a melodic voice */
        SetFreq(voice, pitch, 1);
    else {                                  /* this is a percussive voice */
        if (voice == BD)
                SetFreq(BD, pitch, 0);
        else if (voice == TOM) {
            /* for the last 4 percussions, only the TOM may change */
            /* in frequency, which also modifies the SD */
            SetFreq(TOM, pitch, 0);
            SetFreq(SD, (BYTE)(pitch + TOM_TO_SD), 0);
        }
                
        percBits |= percMasks[voice - BD];
        SndSAmVibRhythm();
    }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | NoteOff | This routine stops playing the note which was
 *     started in <f NoteOn>.
 *
 * @parm BYTE | voice | Specifies which voice to use (0 to 8 in melodic mode,
 *     0 to 10 in percussive mode).
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL NoteOff(BYTE voice)
{
    D3("NoteOff");

    if (!fPercussion || voice < BD)
        SetFreq(voice, notePitch[voice], 0);              /* shut off */
    else {
        percBits &= ~percMasks[voice - BD];
        SndSAmVibRhythm();
    }
}

/***************************************************************************

    private functions

***************************************************************************/

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | ChangePitch | This routine sets the <t halfToneOffset[]> and
 *     <t fNumFreqPtr[]> arrays.  These two global variables are used to
 *     determine the frequency variation to use when a note is played.
 *
 * @parm BYTE | voice | Specifies which voice to use.
 *
 * @parm WORD | pitchBend | Specifies the pitch bend value (0 to 0x3fff,
 *     where 0x2000 is exact tuning).
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL ChangePitch(BYTE voice, WORD pitchBend)
{
static int    oldt1 = -1;
static int    oldHt;
static NPWORD oldPtr;
int           t1;
int           t2;
int           delta;

#if (MID_PITCH != 8192)
#pragma message("ChangePitch: DANGER! C-RUNTIME NOT IN FIXED SEGMENT!!!")
    t1 = (int)(((long)((int)pitchBend - MID_PITCH) * pitchRangeStep)/MID_PITCH);
#else
    DWORD   dw;

    dw = (DWORD)((long)((int)pitchBend - MID_PITCH) * pitchRangeStep);
    t1 = (int)((LOBYTE(HIWORD(dw)) << 8) | HIBYTE(LOWORD(dw))) >> 5;
#endif

    if (oldt1 == t1) {
        fNumFreqPtr[voice] = oldPtr;
        halfToneOffset[voice] = oldHt;
    }

    else {
        if (t1 < 0) {
            t2 = NR_STEP_PITCH - 1 - t1;
            oldHt = halfToneOffset[voice] = -(t2 / NR_STEP_PITCH);
            delta = (t2 - NR_STEP_PITCH + 1) % NR_STEP_PITCH;
            if (delta)
                delta = NR_STEP_PITCH - delta;
        }
        else {
            oldHt = halfToneOffset[voice] = t1 / NR_STEP_PITCH;
            delta = t1 % NR_STEP_PITCH;
        }
    
        oldPtr = fNumFreqPtr[voice] = fNumNotes[delta];
        oldt1 = t1;
    }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSetAllPrm | Transfers all the parameters from slot <p slot>
 *     to the chip.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL SndSetAllPrm(BYTE slot)
{
    D3("SndSetAllPrm");

    SndSAmVibRhythm();
    SndSNoteSel();
    SndSKslLevel(slot);
    SndSFeedFm(slot);
    SndSAttDecay(slot);
    SndSSusRelease(slot);
    SndSAVEK(slot);
    SndWaveSelect(slot);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSAVEK | Sets the AM, VIB, EG-TYP (sustaining), KSR, and
 *     MULTI values.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL SndSAVEK(BYTE slot)
{
BYTE t1;

    t1 = (BYTE)(GetLocPrm(slot, prmAm) ? 0x80 : 0);
    t1 += GetLocPrm(slot, prmVib) ? 0x40 : 0;
    t1 += GetLocPrm(slot, prmStaining) ? 0x20 : 0;
    t1 += GetLocPrm(slot, prmKsr) ? 0x10 : 0;
    t1 += GetLocPrm(slot, prmMulti) & 0xf;
    SndOutput((BYTE)(0x20 | offsetSlot[slot]), t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSFeedFm | Sets the FEEDBACK and FM (connection) values. 
 *     Applicable only to operator 0 for melodic voices.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL SndSFeedFm(BYTE slot)
{
BYTE t1;

    if (operSlot[slot])
        return;
        
    t1 = (BYTE)(GetLocPrm(slot, prmFeedBack) << 1);
    t1 |= GetLocPrm(slot, prmFm) ? 0 : 1;
    SndOutput((BYTE)(0xC0 | voiceSlot[slot]), t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSAttDecay | Sets the ATTACK and DECAY values.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL SndSAttDecay(BYTE slot)
{
BYTE t1;

    t1 = (BYTE)(GetLocPrm(slot, prmAttack) << 4);
    t1 |= GetLocPrm(slot, prmDecay) & 0xf;
    SndOutput((BYTE)(0x60 | offsetSlot[slot]), t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndSSusRelease | Sets the SUSTAIN and RELEASE values.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL SndSSusRelease(BYTE slot)
{
BYTE t1;

    t1 = (BYTE)(GetLocPrm(slot, prmSustain) << 4);
    t1 |= GetLocPrm(slot, prmRelease) & 0xf;
    SndOutput((BYTE)(0x80 | offsetSlot[slot]), t1);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SndWaveSelect | Sets the wave-select parameter.
 *
 * @parm BYTE | slot | Specifies which slot to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR PASCAL SndWaveSelect(BYTE slot)
{
BYTE wave;

    if (modeWaveSel)
        wave = (BYTE)(GetLocPrm(slot, prmWaveSel) & 0x03);
    else
        wave = 0;
    SndOutput((BYTE)(0xE0 | offsetSlot[slot]), wave);
}
