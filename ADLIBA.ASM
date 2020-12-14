        page    60, 132
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   adliba.asm
;
;   Copyright (c) 1991-1992 Microsoft Corporation.  All Rights Reserved.
;   Copyright (c) 2019 Andrey Warkentin <andrey.warkentin@gmail.com>
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        PMODE = 1

        .xlist
        include cmacros.inc                   
        .list

        ?PLM=1                          ; Pascal calling convention
        ?WIN=0                          ; NO! Windows prolog/epilog code


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   debug support
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ifdef DEBUG
    extrn OutputDebugStr:far          ; mmsystem
    extrn _wDebugLevel:word           ; initc.c
endif

D1 macro text
    DOUT 1, < ",13,10,"ADLIB: &text&>
    endm
D2 macro text
    DOUT 2, < &text&>
    endm
D3 macro text
    DOUT 3, < &text&>
    endm
D4 macro text
    DOUT 4, < &text&>
    endm

DOUT macro level, text
    local   string_buffer
    local   wrong_level

ifdef DEBUG

_DATA segment
string_buffer label byte
    db      "&text&", 0
_DATA ends

    cmp     [_wDebugLevel], level
    jl      wrong_level
    push    ds
    push    DataOFFSET string_buffer
    call    OutputDebugStr
wrong_level:
endif
    endm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   assert macros
;
;   AssertF byte        -- fail iff byte==0
;   AssertT byte        -- fail iff byte!=0
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

AssertF     macro exp
    local   assert_ok
ifdef DEBUG
    push    ax
    
    mov     al, exp
    or      al, al
    jnz     assert_ok

    D1      <AssertF fail (&exp&)>
    int     3

assert_ok:
    pop     ax
endif
    endm

AssertT     macro exp
    local   assert_ok
ifdef DEBUG
    push    ax
    
    mov     al, exp
    or      al, al
    jz      assert_ok

    D1      <AssertT fail (&exp&)>
    int     3

assert_ok:
    pop     ax
endif
    endm


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   data segment
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

sBegin Data

        externW <_wPort>                ; address of sound chip

sEnd Data

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   code segment
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ifndef SEGNAME
        SEGNAME equ <_TEXT>
endif

createSeg %SEGNAME, CodeSeg, word, public, CODE

sBegin  CodeSeg

        assumes cs, CodeSeg
        assumes ds, Data

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; @doc INTERNAL 
;
; @api BYTE | inport | Read a port.
;
; @parm WORD | port | The port to read.
;
; @rdesc Returns the port value read.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        assumes ds, Data
        assumes es, nothing

cProc inport <FAR, PUBLIC> <>
cBegin nogen

        mov     dx, [_wPort]
        in      al, dx

        D4 <I:#AL>

        retf

cEnd nogen


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; @doc INTERNAL 
;
; @api void | SndOutput | This function writes data to the chip registers.
;
; @parm BYTE | bRegister | The address of the register to write.
;
; @parm BYTE | bData | The data value to write.
;
; @rdesc There is no return value.
;
; @comm Delay is included after each write.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        assumes ds, Data
        assumes es, nothing

cProc SndOutput <FAR, PUBLIC> <>
        ParmB   bRegister               ; register address
        ParmB   bData                   ; value to write
cBegin

ifdef DEBUG
        mov     al, bRegister
        mov     ah, bData
        D4 <O:#AL,#AH>
endif

        mov     dx, [_wPort]            ; get register select address of sound chip
        mov     al, bRegister           ; register number to write to
        out     dx, al

ifdef OPL_ON_LPT
	inc     dx
	inc	dx
	mov	al, 13
	out     dx, al
	xor 	al, 4
	out	dx, al
	xor	al, 4
	out 	dx, al
endif

        mov     cx, 4                  ; 3.3us delay for OPL2/OPL3
@@:     in      al, dx
        dec     cx
        jnz     @B

ifdef OPL_ON_LPT
	dec	dx
	dec	dx
        mov     al, bData
        out     dx, al
	inc     dx
	inc	dx
	mov	al, 12
	out     dx, al
	xor	al, 4
	out	dx, al
	xor	al, 4
	out 	dx, al
else
        inc     dx                      ; data write address of chip (2/2/1 cycles)
        mov     al, bData               ; value to write (5/4/1 cycles)
        out     dx, al
endif

if OPL-2
        mov     cx, 4                   ; 3.3us delay for OPL3
else
        mov     cx, 23                  ; 23us delay for OPL2
endif
@@:     in      al, dx
        dec     cx 
        jnz     @B 
cEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; @doc INTERNAL
;
; @asm WEP | This function is called when the DLL is unloaded.
;
; @parm WORD | UselessParm | This parameter has no meaning.
;
; @comm WARNING: This function is basically useless since you can't call any
;     kernel function that may cause the LoadModule() code to be reentered.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        assumes ds, nothing
        assumes es, nothing

cProc WEP <FAR, PUBLIC, PASCAL>, <>
;       ParmW   wUselessParm
cBegin nogen
        mov     ax, 1
        retf    2
cEnd nogen

sEnd CodeSeg

        end
