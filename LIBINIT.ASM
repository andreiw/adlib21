        page 60,132
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   libinit.asm
;
;   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
;
;   General Description:
;      Library stub to do local init for a dynamic linked library.
;
;   Restrictions:
;      This must be the first object file in the LINK line.  This assures
;      that the reserved parameter block is at the *base* of DGROUP.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
if1
%out link me first!!
endif

        PMODE = 1

        .xlist
        include cmacros.inc
        include windows.inc
        include vadlibd.inc
        .list

        ?PLM=1                          ; Pascal calling convention
        ?WIN=0                          ; NO! Windows prolog/epilog code

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   equates
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

OFFSEL struc
        off     dw  ?
        sel     dw  ?
OFFSEL ends


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   segmentation
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ifndef SEGNAME
    SEGNAME equ <_TEXT>
endif

createSeg %SEGNAME, CodeSeg, word, public, CODE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   external functions
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        externFP LocalInit           ; in KERNEL
        externNP LibMain             ; C code to do DLL init


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   data segment
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

sBegin Data

        assumes ds, Data

; stuff needed to avoid the C runtime coming in, and init the Windows
; reserved parameter block at the base of DGROUP

        org 0               ; base of DATA segment!

        dd  0               ; so null pointers get 0

maxRsrvPtrs = 5
        dw  maxRsrvPtrs

usedRsrvPtrs = 0
labelDP <PUBLIC, rsrvptrs>

DefRsrvPtr  macro   name
        globalW     name, 0
        usedRsrvPtrs = usedRsrvPtrs + 1
endm

DefRsrvPtr  pLocalHeap          ; local heap pointer
DefRsrvPtr  pAtomTable          ; atom table pointer
DefRsrvPtr  pStackTop           ; top of stack
DefRsrvPtr  pStackMin           ; minimum value of SP
DefRsrvPtr  pStackBot           ; bottom of stack

if maxRsrvPtrs-usedRsrvPtrs
        dw maxRsrvPtrs-usedRsrvPtrs DUP (0)
endif

public  __acrtused
        __acrtused = 1

        externW <_wPort>                ; address of sound chip


        public VADLIBD_Entry
VADLIBD_Entry   dd  0

sEnd Data

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   code segment
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

sBegin CodeSeg

        assumes cs, CodeSeg

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; @doc INTERNAL
;
; @asm LibEntry | Called when DLL is loaded.
;
; @reg  CX | Size of heap.
;
; @reg  DI | Module handle.
;
; @reg  DS | Automatic data segment.
;
; @reg  ES:SI | Address of command line (not used).
;
; @rdesc AX is TRUE if the load is successful and FALSE otherwise.
;
; @comm Registers preserved are SI,DI,DS,BP.  Registers destroyed are
;       AX,BX,CX,DX,ES,FLAGS.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
cProc LibEntry <FAR, PUBLIC, NODATA>, <>
cBegin

        ; push frame for LibMain (hModule, cbHeap, lpszCmdLine)

        push di
        push cx
        push es
        push si

        ; init the local heap (if one is declared in the .def file)

        jcxz no_heap

        cCall LocalInit, <0, 0, cx>

no_heap:
        cCall LibMain

cEnd


;---------------------------------------------------------------------------;
;
;   vadlibdGetEntryPoint
;
;   DESCRIPTION:
;
;
;---------------------------------------------------------------------------;

        assumes ds, Data
        assumes es, nothing

cProc vadlibdGetEntryPoint <FAR, PASCAL, PUBLIC> <si, di>
cBegin

        xor     di, di                  ; zero ES:DI before call
        mov     es, di
        mov     ax, 1684h               ; get device API entry point
        mov     bx, VADLIBD_Device_ID   ; virtual device ID
        int     2Fh                     ; call WIN/386 INT 2F API
        mov     ax, es                  ; return farptr
        or      ax, di
        jz      gvadlibd_exit
        mov     word ptr [VADLIBD_Entry.off], di
        mov     word ptr [VADLIBD_Entry.sel], es

gvadlibd_exit:
cEnd


;---------------------------------------------------------------------------;
;
;   WORD FAR PASCAL vadlibdAcquireAdLibSynth( void )
;
;   DESCRIPTION:
;
;   ENTRY:
;
;   EXIT:
;       IF Carry Clear
;           AX = 0, success--go ahead and open
;       ELSE
;           carry set, AX = error code:
;               ADLIB_AS_Err_Bad_Synth      equ 01h
;               ADLIB_AS_Err_Already_Owned  equ 02h
;
;   USES:
;       Flags, AX, BX, DX
;
;---------------------------------------------------------------------------;

        assumes ds, Data
        assumes es, nothing

cProc vadlibdAcquireAdLibSynth <FAR, PASCAL, PUBLIC> <>
cBegin

        mov     ax, [VADLIBD_Entry.off]     ; Q: is VADLIBD installed?
        or      ax, [VADLIBD_Entry.sel]
        jz      vadlibd_Acquire_Exit        ;   N: then leave (return 0)


;- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -;
;       AX = Base of Syth to acquire (for example, 0388h)
;       BX = Flags, should be zero.
;       DX = ADLIB_API_Acquire_Synth (1)
;- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -;

        mov     ax, [_wPort]                ; base port to acquire
        xor     bx, bx
        mov     dx, ADLIB_API_Acquire_Synth
        call    [VADLIBD_Entry]

vadlibd_Acquire_Exit:

cEnd


;---------------------------------------------------------------------------;
;
;   WORD FAR PASCAL vadlibdReleaseAdLibSynth( void )
;
;   DESCRIPTION:
;
;   ENTRY:
;
;   EXIT:
;       IF Carry Clear
;           AX = 0, success--go ahead and open
;       ELSE
;           carry set, AX = error code:
;               ADLIB_RS_Err_Bad_Synth      equ 01h
;               ADLIB_RS_Err_Not_Yours      equ 02h
;
;   USES:
;       Flags, AX, BX, DX
;
;---------------------------------------------------------------------------;

        assumes ds, Data
        assumes es, nothing

cProc vadlibdReleaseAdLibSynth <FAR, PASCAL, PUBLIC> <>
cBegin

        mov     ax, [VADLIBD_Entry.off]     ; Q: is VADLIBD installed?
        or      ax, [VADLIBD_Entry.sel]
        jz      vadlibd_Release_Exit        ;   N: then leave (return 0)


;- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -;
;       AX = Base of synth to release (for example, 0388h)
;       BX = Flags should be zero.
;       DX = ADLIB_API_Release_Synth (2)
;- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -;

        mov     ax, [_wPort]                ; base port to acquire
        xor     bx, bx
        mov     dx, ADLIB_API_Release_Synth
        call    [VADLIBD_Entry]

vadlibd_Release_Exit:

cEnd

sEnd CodeSeg

        end LibEntry
