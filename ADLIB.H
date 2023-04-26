/****************************************************************************
 *
 *   adlib.h
 *
 *   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
 *   Copyright (c) 2019-2020 Andrei Warkentin <andrey.warkentin@gmail.com>
 *
 ***************************************************************************/

/* this way we get more symbols for debug */
#ifdef DEBUG
#define static
#endif

#define _TEXTIFY(A) #A
#define TEXTIFY(A) _TEXTIFY(A)

#define DRIVER_VERSION  1;

#ifndef DEF_PORT
#ifdef OPL_ON_LPT
#define DEF_PORT      0x378              /* LPT1 */
#else
#define DEF_PORT      0x388              /* default port address */
#endif
#endif

#define NUMVOICES     (fPercussion?11:9) /* number of voices we have */
#define NUMMELODIC    (fPercussion?6:9)  /* number of melodic voices */
#define NUMPERCUSSIVE (fPercussion?5:0)  /* number of percussive voices */

#define NUMCHANNELS    16                /* number of MIDI channels */
#define DRUMKITCHANNEL 15                /* our drum kit channel */

#define MAXPATCH      180                /* max patch number (>127 for drums) */
#define MAXVOLUME    0x7f
#define NUMLOCPARAM    14                /* number of loc params per voice */

#define FIRSTDRUMNOTE  35
#define LASTDRUMNOTE   81
#define NUMDRUMNOTES   (LASTDRUMNOTE - FIRSTDRUMNOTE + 1)

#define MAX_PITCH    0x3fff             /* maximum pitch bend value */
#define MID_PITCH    0x2000             /* mid pitch bend value (no shift) */
#define NR_STEP_PITCH    25             /* steps per half-tone for pitch bend */
#define MID_C            60             /* MIDI standard mid C */
#define CHIP_MID_C       48             /* sound chip mid C */

#define RT_BANK         256
#define RT_DRUMKIT      257
#define DEFAULTBANK     1
#define DEFAULTDRUMKIT  1

#define ISSTATUS(bData)         ((bData) & 0x80)
#define FILTERCHANNEL(bStatus)  ((BYTE)((bStatus) & 0xf0))
#define FILTERSTATUS(bStatus)   ((BYTE)((bStatus) & 0x0f))

#define STATUS_NOTEOFF          0x80
#define STATUS_NOTEON           0x90
#define STATUS_POLYPHONICKEY    0xa0
#define STATUS_CONTROLCHANGE    0xb0
#define STATUS_PROGRAMCHANGE    0xc0
#define STATUS_CHANNELPRESSURE  0xd0
#define STATUS_PITCHBEND        0xe0

#define STATUS_SYS              0xf0
#define STATUS_SYSEX            0xf0
#define STATUS_QFRAME           0xf1
#define STATUS_SONGPOINTER      0xf2
#define STATUS_SONGSELECT       0xf3
#define STATUS_F4               0xf4
#define STATUS_F5               0xf5
#define STATUS_TUNEREQUEST      0xf6
#define STATUS_EOX              0xf7
#define STATUS_TIMINGCLOCK      0xf8
#define STATUS_F9               0xf9
#define STATUS_START            0xfa
#define STATUS_CONTINUE         0xfb
#define STATUS_STOP             0xfc
#define STATUS_FD               0xfd
#define STATUS_ACTIVESENSING    0xfe
#define STATUS_SYSTEMRESET      0xff

/* parameters of each voice */
#define prmKsl          0      /* key scale level (KSL) - level controller */
#define prmMulti        1      /* frequency multiplier (MULTI) - oscillator */
#define prmFeedBack     2      /* modulation feedback (FB) - oscillator */
#define prmAttack       3      /* attack rate (AR) - envelope generator */
#define prmSustain      4      /* sustain level (SL) - envelope generator */
#define prmStaining     5      /* sustaining sound (SS) - envelope generator */
#define prmDecay        6      /* decay rate (DR) - envelope generator */
#define prmRelease      7      /* release rate (RR) - envelope generator */
#define prmLevel        8      /* output level (OL) - level controller */
#define prmAm           9      /* amplitude vibrato (AM) - level controller */
#define prmVib          10     /* frequency vibrator (VIB) - oscillator */
#define prmKsr          11     /* envelope scaling (KSR) - envelope generator */
#define prmFm           12     /* fm=0, additive=1 (FM) (operator 0 only) */
#define prmWaveSel      13     /* wave select */

/* global parameters */
#define prmAmDepth      14
#define prmVibDepth     15
#define prmNoteSel      16
#define prmPercussion   17

/* percussive voice numbers (0-5 are melodic voices, 12 op): */
#define BD              6         /* bass drum (2 op) */
#define SD              7         /* snare drum (1 op) */
#define TOM             8         /* tom-tom (1 op) */
#define CYMB            9         /* cymbal (1 op) */
#define HIHAT           10        /* hi-hat (1 op) */

/*
 * In percussive mode, the last 4 voices (SD TOM HH CYMB) are created
 * using melodic voices 7 & 8. A noise generator uses channels 7 & 8
 * frequency information for creating rhythm instruments. Best results
 * are obtained by setting TOM two octaves below mid C and SD 7 half-tones
 * above TOM. In this implementation, only the TOM pitch may vary, with the
 * SD always set 7 half-tones above.
 */
#define TOM_PITCH        24      /* best frequency, in range of 0 to 95 */
#define TOM_TO_SD        7       /* 7 half-tones between voice 7 & 8 */
#define SD_PITCH         (TOM_PITCH + TOM_TO_SD)

#define BANK_SIG_LEN        6
#define BANK_FILLER_SIZE    8

typedef BYTE huge *HPBYTE;

/* instrument bank file header */
typedef struct {
    char      majorVersion;
    char      minorVersion;
    char      sig[BANK_SIG_LEN];          /* signature: "ADLIB-" */
    WORD      nrDefined;                  /* number of list entries used */
    WORD      nrEntry;                    /* number of total entries in list */
    long      offsetIndex;                /* offset of start of list of names */
    long      offsetTimbre;               /* offset of start of data */
    char      filler[BANK_FILLER_SIZE];   /* must be zero */
} BANKHDR, NEAR *NPBANKHDR, FAR *LPBANKHDR;

typedef BYTE NEAR * NPBYTE;
typedef WORD NEAR * NPWORD;

typedef struct {
  BYTE ksl;            /* KSL */
  BYTE freqMult;       /* MULTI */
  BYTE feedBack;       /* FB */
  BYTE attack;         /* AR */
  BYTE sustLevel;      /* SL */
  BYTE sustain;        /* SS */
  BYTE decay;          /* DR */
  BYTE release;        /* RR */
  BYTE output;         /* OL */
  BYTE am;             /* AM */
  BYTE vib;            /* VIB */
  BYTE ksr;            /* KSR */
  BYTE fm;             /* FM */
} OPERATOR, NEAR *NPOPERATOR, FAR *LPOPERATOR;

typedef struct {
  BYTE      mode;             /* 0 = melodic, 1 = percussive */
  BYTE      percVoice;        /* if mode == 1, voice number to be used */
  OPERATOR  op0;
  OPERATOR  op1;
  BYTE      wave0;            /* waveform for operator 0 */
  BYTE      wave1;            /* waveform for operator 1 */
} TIMBRE, NEAR *NPTIMBRE, FAR *LPTIMBRE;

typedef struct drumpatch_tag {
  BYTE patch;                 /* the patch to use */
  BYTE note;                  /* the note to play  */
} DRUMPATCH;

/* client information structure */
typedef struct synthalloc_tag {
  HANDLE       hMidiOut;      /* handle for our parent in MMSYSTEM */
  DWORD        dwCallback;    /* callback for client */
  DWORD        dwInstance;    /* DWORD of reference data from client */
  DWORD        dwFlags;       /* allocation flags */
} SYNTHALLOC, NEAR *NPSYNTHALLOC;

typedef struct _MIDIMSG {
  BYTE ch;                    /* channel number */
  BYTE b1;                    /* first data byte */
  BYTE b2;                    /* second data byte */
  BYTE pad;                   /* not used */
} MIDIMSG, FAR *LPMIDIMSG;

/* midic.c */
extern DWORD FAR PASCAL _loadds modMessage(WORD id, WORD msg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2);
extern BYTE bCurrentLen;
extern BYTE status;

/* midimain.c */
extern void NEAR PASCAL synthMidiData(HPBYTE lpBuf, DWORD dwLength);
extern void NEAR PASCAL synthAllNotesOff(void);

/* adlib.c */
extern BOOL FAR PASCAL vadlibdGetEntryPoint( void );
extern WORD FAR PASCAL vadlibdAcquireAdLibSynth( void );
extern WORD FAR PASCAL vadlibdReleaseAdLibSynth( void );

extern void FAR PASCAL SetSlotParam(BYTE slot, NPBYTE param, BYTE waveSel);
extern void FAR PASCAL SetFreq(BYTE voice, BYTE pitch, BYTE keyOn);
extern void FAR PASCAL SndSAmVibRhythm(void);
extern void FAR PASCAL SndSNoteSel(void);
extern void NEAR PASCAL SndSKslLevel(BYTE slot);
extern void NEAR PASCAL SetVoiceTimbre(BYTE voice, NPOPERATOR pOper0);
extern void NEAR PASCAL SetVoicePitch(BYTE voice, WORD pitchBend);
extern void NEAR PASCAL SetVoiceVolume(BYTE voice, BYTE volume);
extern void NEAR PASCAL NoteOn(BYTE voice, BYTE pitch);
extern void NEAR PASCAL NoteOff(BYTE voice);

extern BYTE slotRelVolume[18];    /* relative volume of slots */
extern BYTE percBits;             /* control bits of percussive voices */
extern BYTE amDepth;              /* chip global parameters ... */
extern BYTE vibDepth;             /* ... */
extern BYTE noteSel;              /* ... */
extern BYTE modeWaveSel;          /* != 0 if used with 'wave-select' parms */
extern BOOL fPercussion;          /* percussion mode parameter */
extern int  pitchRangeStep;       /* == pitchRange * NR_STEP_PITCH */
extern WORD   fNumNotes[NR_STEP_PITCH] [12];
extern NPWORD fNumFreqPtr[11];    /* lines of fNumNotes table (one per voice) */
extern int  halfToneOffset[11];   /* one per voice */
extern BYTE noteDIV12[96];        /* table of (0..95) DIV 12 */
extern BYTE noteMOD12[96];        /* table of (0..95) MOD 12 */
extern BYTE offsetSlot[];         /* offset of each slot */
extern BYTE operSlot[];           /* operator (0 or 1) in each slot */

extern BYTE gbMidiLengths[];
extern BYTE gbSysLengths[];

#define MIDILENGTH(bStatus)     (gbMidiLengths[((bStatus) & 0x70) >> 4])
#define SYSLENGTH(bStatus)      (gbSysLengths[(bStatus) & 0x07])

/* init.c */
extern BOOL NEAR PASCAL Enable(void);
extern void NEAR PASCAL Disable(void);
extern int NEAR PASCAL LibMain(HANDLE hInstance, WORD wHeapSize, LPSTR lpCmdLine);

extern WORD      wPort;                    /* card's port address */
extern WORD      wDebugLevel;              /* debug level */
extern BOOL      fEnabled;                 /* have we successfully enabled? */
extern TIMBRE    patches[MAXPATCH];        /* melodic patch information */
extern DRUMPATCH drumpatch[NUMDRUMNOTES];  /* drumkit patch information */

/* adliba.asm */
extern BYTE FAR PASCAL inport(void);
extern void FAR PASCAL SndOutput(BYTE addr, BYTE dataVal);

#ifndef NOSTR
extern char far aszDriverName[];
extern char far aszProductName[];
extern char far aszSystemIni[];
#ifdef DEBUG
extern char far aszAdlib[];
extern char far aszMMDebug[];
#endif
#endif /* NOSTR */

#ifdef DEBUG
#define D1(sz) if (wDebugLevel >= 1) (OutputDebugStr("\r\nADLIB: "),OutputDebugStr(sz))
#define D2(sz) if (wDebugLevel >= 2) (OutputDebugStr(" "),OutputDebugStr(sz))
#define D3(sz) if (wDebugLevel >= 3) (OutputDebugStr(" "),OutputDebugStr(sz))
#define D4(sz) if (wDebugLevel >= 4) (OutputDebugStr(" "),OutputDebugStr(sz))
#else
#define D1(sz) 0
#define D2(sz) 0
#define D3(sz) 0
#define D4(sz) 0
#endif
