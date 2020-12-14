/****************************************************************************
 *
 *   init.c
 *
 *   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
 *   Copyright (c) 2019-2020 Andrei Warkentin <andrey.warkentin@gmail.com>
 *
 ***************************************************************************/

#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#define  NOSTR                  /* to avoid redefining the strings */
#include "adlib.h"

void FAR SoundWarmInit(void);

static int  NEAR SoundColdInit(void);
static int  NEAR BoardInstalled(void);
static void NEAR SetMode(BYTE mode);
static void NEAR SetGParam(BYTE amD, BYTE vibD, BYTE nSel);
static void NEAR Set3812(BYTE state);
static void NEAR InitSlotVolume(void);
static void NEAR InitFNums(void);
static void NEAR SoundChut(BYTE voice);
static void NEAR SetPitchRange(WORD pR);
static void NEAR SetFNum(NPWORD fNumVec, int num, int den);
static long NEAR CalcPremFNum(int numDeltaDemiTon, int denDeltaDemiTon);
static int  NEAR PASCAL LoadPatches(void);
static int  NEAR PASCAL LoadDrumPatches(void);

WORD      wPort = DEF_PORT;             /* address of sound chip */
BOOL      fEnabled;                     /* are we enabled? */
TIMBRE    patches[MAXPATCH];            /* patch data  */
DRUMPATCH drumpatch[NUMDRUMNOTES];      /* drum kit data */

#ifdef DEBUG
WORD      wDebugLevel;                  /* debug level */
#endif

#define BCODE _based(_segname("_CODE"))

/* non-localized strings */
char BCODE aszDriverName[]  = "adlib21.drv";
char BCODE aszProductName[] = "Ad Lib";
char BCODE aszSystemIni[]   = "system.ini";
#ifdef DEBUG
char BCODE aszAdlib[]   = "adlib";
char BCODE aszMMDebug[] = "mmdebug";
#endif

static HANDLE ghInstance;           /* our global instance */
static BOOL   fInit;                /* have we initialized yet? */

/* format of drumkit.bin file */
typedef struct drumfilepatch_tag {
    BYTE key;                   /* the key to map */
    BYTE patch;                 /* the patch to use */
    BYTE note;                  /* the note to play  */
} DRUMFILEPATCH, *NPDRUMFILEPATCH, FAR *LPDRUMFILEPATCH;

/****************************************************************************
 * @doc INTERNAL
 *
 * @api int | SoundColdInit | Must be called for start-up initialization.
 *
 * @rdesc Returns a nonzero value if the board is installed and zero otherwise.
 ***************************************************************************/
static int NEAR
SoundColdInit(void)
{
  int hardware;

  D1("SoundColdInit");

  if (hardware = BoardInstalled()) {
    SoundWarmInit();
  }

  return hardware;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SoundWarmInit | Initializes the chip in melodic mode (mode == 0).
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void FAR
SoundWarmInit(void)
{
  BYTE i;

  D1("SoundWarmInit");

  SetGParam(0, 0, 0);      /* init global parameters */
  InitSlotVolume();        /* sets volume of each slot to MAXVOLUME */
  InitFNums();             /* initializes frequency shift table to no shift */
  for (i = 0 ; i <= 8; i++) {
    SoundChut(i);        /* set frequencies of voices 0 - 8 to 0 */
  }
  SetMode(1);              /* percussion mode (melodic mode == 0) */
  SetPitchRange(2);        /* GMM pitch range is 2 semitones */

#if 1
  Set3812(1);              /* sets wave-select parameter */
#else
  Set3812(0);              /* DOES NOT set wave-select parameter */
#endif
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api int | BoardInstalled | Checks to see if the board is installed.
 *
 * @rdesc Returns a nonzero value if the board is installed and zero otherwise.
 ***************************************************************************/
static int NEAR
BoardInstalled(void)
{
  BYTE t1, t2, i;

  D1("BoardInstalled");

  SndOutput(4, 0x60);             /* mask T1 & T2 */
  SndOutput(4, 0x80);             /* reset IRQ */
  t1 = inport();                  /* read status register */
  SndOutput(2, 0xff);             /* set timer - 1 latch */
  SndOutput(4, 0x21);             /* unmask & start T1 */
  for (i = 0; i < 80; i++) {      /* At least 80 uSec delay */
    inport();
  }
  t2 = inport();                  /* read status register */
  SndOutput(4, 0x60);
  SndOutput(4, 0x80);

  return (t1 & 0xE0) == 0 && (t2 & 0xE0) == 0xC0;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetMode | Puts the chip in melodic mode (mode == 0), or in
 *     percussive mode (mode != 0).
 *
 * @parm BYTE | mode | Specifies which mode to put the chip into.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
SetMode(BYTE mode)
{
  if (mode) {
    SetFreq(TOM, TOM_PITCH, 0); /* set frequency of TOM voice */
    SetFreq(SD, SD_PITCH, 0);   /* set frequency of SD voice */
  }

  fPercussion = mode;
  percBits = 0;                 /* initialize control bits of percussive voices */
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetGParam | Sets the 3 global parameters AmDepth, VibDepth
 *     and NoteSel.  The change takes place immediately.
 *
 * @parm BYTE | amD | The new AmDepth parameter.
 *
 * @parm BYTE | vibD | The new VibDepth parameter.
 *
 * @parm BYTE | nSel | The new NoteSel parameter.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
SetGParam(BYTE amD,
          BYTE vibD,
          BYTE nSel)
{
  amDepth = amD;
  vibDepth = vibD;
  noteSel = nSel;

  SndSAmVibRhythm();
  SndSNoteSel();
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | Set3812 | Enables (state != 0) or disables (state == 0) the
 *     wave-select parameters.
 *
 * @parm BYTE | state | Indicates whether to enable or disable the wave-select
 *     parameters.
 *
 * @comm If you do not want to use the wave-select parameters, call this
 *     function with a value of 0 AFTER calling SoundColdInit() or
 *     SoundWarmInit().
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
Set3812(BYTE state)
{
  BYTE i;

  D1("Set3812");

  /* set waveform for each of the 18 slots to sine wave */
  for (i = 0; i < 18; i++) {
    SndOutput((BYTE)(0xE0 | offsetSlot[i]), 0);
  }

  /* enable/disable the wave-select parameters */
  modeWaveSel = (BYTE)(state ? 0x20 : 0);
  SndOutput(1, modeWaveSel);
}

#if 0 /* never used */
static void NEAR
InitSlotParams(void);
/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | InitSlotParams | In melodic mode, this function initializes all
 *     voices to electric-pianos.  In percussive mode, it initializes the 6
 *     melodic voices to electric-pianos and the 5 percussive voices to their
 *     default timbres.
 *
 * @comm This function is pointless because the timbre of each voice gets
 *     set as soon as the voice is allocated, so it's commented out.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
InitSlotParams(void)
{
  BYTE i;

  /* definition of default melodic(electric piano) and percussive voices: */
  static BYTE pianoOpr0[] = { 1,  1, 3, 15,  5, 0, 1,  3, 15, 0, 0, 0, 1, 0 };
  static BYTE pianoOpr1[] = { 0,  1, 1, 15,  7, 0, 2,  4,  0, 0, 0, 1, 0, 0 };
  static BYTE bdOpr0[]    = { 0,  0, 0, 10,  4, 0, 8, 12, 11, 0, 0, 0, 1, 0 };
  static BYTE bdOpr1[]    = { 0,  0, 0, 13,  4, 0, 6, 15,  0, 0, 0, 0, 1, 0 };
  static BYTE sdOpr[]     = { 0, 12, 0, 15, 11, 0, 8,  5,  0, 0, 0, 0, 0, 0 };
  static BYTE tomOpr[]    = { 0,  4, 0, 15, 11, 0, 7,  5,  0, 0, 0, 0, 0, 0 };
  static BYTE cymbOpr[]   = { 0,  1, 0, 15, 11, 0, 5,  5,  0, 0, 0, 0, 0, 0 };
  static BYTE hhOpr[]     = { 0,  1, 0, 15, 11, 0, 7,  5,  0, 0, 0, 0, 0, 0 };

  for (i = 0; i < 18; i++) {
    if (operSlot[i]) {
      SetSlotParam(i, pianoOpr1, 0);
    } else {
      SetSlotParam(i, pianoOpr0, 0);
    }
  }

  if (fPercussion) {
    SetSlotParam(12, bdOpr0, 0);
    SetSlotParam(15, bdOpr1, 0);
    SetSlotParam(16, sdOpr, 0);
    SetSlotParam(14, tomOpr, 0);
    SetSlotParam(17, cymbOpr, 0);
    SetSlotParam(13, hhOpr, 0);
  }
}
#endif

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | InitSlotVolume | Sets the volume values in the <t slotRelVolume>
 *     array to MAXVOLUME.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
InitSlotVolume(void)
{
  int i;

  for (i = 0; i < 18; i++) {
    slotRelVolume[i] = MAXVOLUME;
  }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | InitFNums | Initializes all lines of the frequency table
 *     (the <p fNumNotes> array). Each line represents 12 half-tones shifted
 *     by (n / NR_STEP_PITCH), where 'n' is the line number and ranges from
 *     1 to NR_STEP_PITCH.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
InitFNums(void)
{
  WORD i, j, k;
  WORD num;           /* numerator */
  WORD numStep;       /* step value for numerator */
  WORD row;           /* row in the frequency table */

  /* calculate each row in the fNumNotes table */
  numStep = 100 / NR_STEP_PITCH;
  for (num = row = 0; row < NR_STEP_PITCH; row++, num += numStep) {
    SetFNum(fNumNotes[row], num, 100);
  }

  /* fNumFreqPtr has an element for each voice, pointing to the  */
  /* appropriate row in the fNumNotes table.  They're all initialized */
  /* to the first row, which represents no pitch shift. */
  for (i = 0; i < 11; i++) {
    fNumFreqPtr[i] = fNumNotes[0];
    halfToneOffset[i] = 0;
  }

  /* just for optimization */
  for (i = 0, k = 0; i < 8; i++) {
    for (j = 0; j < 12; j++, k++) {
      noteDIV12[k] = (BYTE)i;
      noteMOD12[k] = (BYTE)j;
    }
  }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SoundChut | Sets the frequency of voice <p voice> to 0 Hz.
 *
 * @parm BYTE | voice | Specifies which voice to set.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
SoundChut(BYTE voice)
{
  D1("SoundChut");

  SndOutput((BYTE)(0xA0 | voice), 0);
  SndOutput((BYTE)(0xB0 | voice), 0);
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetPitchRange | This routine changes the global pitch bend
 *      range value.
 *
 * @parm WORD | pR | The new pitch bend range.
 *
 * @comm The value can be from 1 to 12 (in half-tones).  For example, the
 *     value 12 means that the pitch bend will range from -12 (pitchBend == 0,
 *     see <f xSetVoicePitch>) to +12 (pitchBend == 0x3fff) half-tones.  The
 *     change will be effective as of the next call to <f xSetVoicePitch>.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
SetPitchRange(WORD pR)
{
  if (pR > 12) {
    pR = 12;
  } else if (pR < 1) {
    pR = 1;
  }
  pitchRangeStep = pR * NR_STEP_PITCH;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | SetFNum | Initializes a line in the frequency table with
 *     shifted frequency values.  The values are shifted a fraction (num/den)
 *     of a half-tone.
 *
 * @parm NPWORD | fNumVec | The line from the frequency table.
 *
 * @parm int | num | Numerator.
 *
 * @parm int | den | Denominator.
 *
 * @xref CalcPremFNum
 *
 * @rdesc There is no return value.
 ***************************************************************************/
static void NEAR
SetFNum(NPWORD fNumVec,
        int num,
        int den)
{
  int  i;
  long val;

  *fNumVec++ = (WORD)((4 + (val = CalcPremFNum(num, den))) >> 3);
  for (i = 1; i < 12; i++) {
    val *= 106;
    *fNumVec++ = (WORD)((4 + (val /= 100)) >> 3);
  }
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api long | CalcPremFNum | Calculates some magic number that is used in
 *     setting the values in the <p fNumNotes> table.
 *
 * @parm int | numDeltaDemiTon | Numerator (-100 to +100).
 *
 * @parm int | denDeltaDemiTon | Denominator (1 to 100).
 *
 * @comm If the numerator (numDeltaDemiTon) is positive, the frequency is
 *     increased; if negative, it is decreased.  The function calculates:
 *         f8 = Fb(1 + 0.06 num /den)          (where Fb = 26044 * 2 / 25)
 *         fNum8 = f8 * 65536 * 72 / 3.58e6
 *
 * @rdesc Returns fNum8, which is the binary value of the frequency 260.44 (C)
 *     shifted by +/- <p numdeltaDemiTon> / <p denDeltaDemiTon> * 8.
 ***************************************************************************/
static long NEAR
CalcPremFNum(int numDeltaDemiTon,
             int denDeltaDemiTon)
{
  long f8;
  long fNum8;
  long d100;

  d100 = denDeltaDemiTon * 100;
  f8 = (d100 + 6 * numDeltaDemiTon) * (26044L * 2L);
  f8 /= d100 * 25;

  fNum8 = f8 * 16384;
  fNum8 *= 9L;
  fNum8 /= 179L * 625L;

  return fNum8;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api int | LoadPatches | Reads the patch set from the BANK resource and
 *     builds the <p patches> array.
 *
 * @rdesc Returns the number of patches loaded, or 0 if an error occurs.
 ***************************************************************************/
static int NEAR PASCAL
LoadPatches(void)
{
  HANDLE hResInfo;
  HANDLE hResData;
  LPSTR  lpRes;
  int    iPatches;
  DWORD  dwOffset;
  DWORD  dwResSize;
  LPTIMBRE  lpBankTimbre;
  LPTIMBRE  lpPatchTimbre;
  LPBANKHDR lpBankHdr;

  /* find resource and get its size */
  hResInfo = FindResource(ghInstance, MAKEINTRESOURCE(DEFAULTBANK), MAKEINTRESOURCE(RT_BANK));
  if (!hResInfo) {
    D1("Default bank resource not found");
    return 0;
  }
  dwResSize = (DWORD)SizeofResource(ghInstance, hResInfo);

  /* load and lock resource */
  hResData = LoadResource(ghInstance, hResInfo);
  if (!hResData) {
    D1("Bank resource not loaded");
    return 0;
  }
  lpRes = LockResource(hResData);
  if (!lpRes) {
    D1("Bank resource not locked");
    return 0;
  }

  /* read the bank resource, loading patches as we find them */

  D1("loading patches");
  lpBankHdr = (LPBANKHDR)lpRes;
  dwOffset = lpBankHdr->offsetTimbre;                /* point to first one */

  for (iPatches = 0; iPatches < MAXPATCH; iPatches++) {
    lpBankTimbre = (LPTIMBRE)(lpRes + dwOffset);
    lpPatchTimbre = &patches[iPatches];
    *lpPatchTimbre = *lpBankTimbre;

    dwOffset += sizeof(TIMBRE);
    if (dwOffset + sizeof(TIMBRE) > dwResSize) {
      D1("Attempt to read past end of bank resource");
      break;
    }
  }

  UnlockResource(hResData);
  FreeResource(hResData);

  return iPatches;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api int | LoadDrumPatches | Reads the drum kit patch set from the 
 *     DRUMKIT resource and builds the <p drumpatch> array. 
 *
 * @comm Each entry of the <t drumpatch> array (representing a key number
 *     from the "drum patch") consists of a patch number and note number
 *     from some other patch.
 *
 * @rdesc Returns the number of patches loaded, or 0 if an error occurs.
 ***************************************************************************/
static int NEAR PASCAL
LoadDrumPatches(void)
{
  HANDLE hResInfo;
  HANDLE hResData;
  LPSTR  lpRes;
  int    iPatches;
  int    key;
  DWORD  dwOffset;
  DWORD  dwResSize;
  LPDRUMFILEPATCH lpResPatch;

  /* find resource and get its size */
  hResInfo = FindResource(ghInstance, MAKEINTRESOURCE(DEFAULTDRUMKIT), MAKEINTRESOURCE(RT_DRUMKIT));
  if (!hResInfo) {
    D1("Default drum resource not found");
    return 0;
  }
  dwResSize = (DWORD)SizeofResource(ghInstance, hResInfo);

  /* load and lock resource */
  hResData = LoadResource(ghInstance, hResInfo);
  if (!hResData) {
    D1("Drum resource not loaded");
    return 0;
  }
  lpRes = LockResource(hResData);
  if (!lpRes) {
    D1("Drum resource not locked");
    return 0;
  }

  /* read the drum resource, loading patches as we find them */

  D1("reading drum data");
  dwOffset = 0;
  for (iPatches = 0; iPatches < NUMDRUMNOTES; iPatches++) {
    lpResPatch = (LPDRUMFILEPATCH)(lpRes + dwOffset);
    key = lpResPatch->key;
    if ((key >= FIRSTDRUMNOTE) && (key <= LASTDRUMNOTE)) {
      drumpatch[key - FIRSTDRUMNOTE].patch = lpResPatch->patch;
      drumpatch[key - FIRSTDRUMNOTE].note = lpResPatch->note;
    } else {
      D1("Drum patch key out of range");
    }

    dwOffset += sizeof(DRUMFILEPATCH);
    if (dwOffset + sizeof(DRUMFILEPATCH) > dwResSize) {
      D1("Attempt to read past end of drum resource");
      break;
    }
  }

  UnlockResource(hResData);
  FreeResource(hResData);

  return iPatches;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api BOOL | Enable | Enables the card.  If we haven't yet enabled in
 *     this session, it will do a cold restart of the card and load the
 *     patches; otherwise it will do a warm restart.
 *
 * @rdesc Returns TRUE if successful and false otherwise.
 ***************************************************************************/
BOOL NEAR PASCAL
Enable(void)
{
  D1("Enable");

  if (vadlibdAcquireAdLibSynth()) {
    D1("AdLib could NOT be aquired for ENABLE!!!");
    return FALSE;
  }

  if (!fInit) {                        /* if we haven't initialized yet */
    if (!SoundColdInit()) {            /* if we can't find a card */
      return FALSE;                    /* keep fInit set to FALSE */
    }

    if (!LoadPatches()) {              /* load the melodic patches */
      return FALSE;
    }

    if (!LoadDrumPatches()) {          /* load the drum kit information */
      return FALSE;
    }
  } else {                             /* we've already initialized */
    SoundWarmInit();                   /* so do a warm restart */
  }

  fInit = TRUE;
  fEnabled = TRUE;

  if (vadlibdReleaseAdLibSynth()) {
    D1("AdLib could NOT be RELEASED for ENABLE!!! VERY GOOFY!!");
  }

  return TRUE;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api void | Disable | Since this function is called either when a
 *     Windows session ends or when we switch to a DOS box (in 286 mode),
 *     we'll reset the card in preparation for someone else to use it.
 *
 * @rdesc There is no return value.
 ***************************************************************************/
void NEAR PASCAL
Disable(void)
{
  D1("Disable");

  if (fInit) {
    if (vadlibdAcquireAdLibSynth()) {
      D1("AdLib could NOT be aquired for DISABLE!!!");
    }

    /* reset card to be good */
    SoundWarmInit();

    if (vadlibdReleaseAdLibSynth()) {
      D1("AdLib could NOT be RELEASED for DISABLE!!!");
    }
  }

  fEnabled = FALSE;
}

/****************************************************************************
 * @doc INTERNAL
 *
 * @api int | LibMain | Library initialization code.
 *
 * @parm HANDLE | hInstance | Our instance handle.
 *
 * @parm WORD | wHeapSize | The heap size from the .def file.
 *
 * @parm LPSTR | lpCmdLine | The command line.
 *
 * @rdesc Returns 1 if the initialization was successful and 0 otherwise.
 ***************************************************************************/

int NEAR PASCAL
LibMain(HANDLE hInstance,
        WORD wHeapSize,
        LPSTR lpCmdLine)
{
#ifdef DEBUG
  /* get debug level - default is 0 */
  wDebugLevel = GetProfileInt(aszMMDebug, aszAdlib, 0);
#endif

  D1("LibMain");

  ghInstance = hInstance;

  vadlibdGetEntryPoint();

  return 1;
}
