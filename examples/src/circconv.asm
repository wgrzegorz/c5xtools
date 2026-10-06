*         ___|       |               |
*     __| __ \ \  / __|  _ \   _ \  |  __|
*    (      ) |`  <  |   (   | (   | |\__ \ 
*   \___|____/ _/\_\\__|\___/ \___/ _|____/
* ======================================dSc==
*   Brought to you by + Cult of the Modem +
*
* TMS320C5x assembler/linker/disassembler/hex
*
* SPDX-License-Identifier: MIT
* Author:  Grzegorz Worona <grzegorz@worona.pl>
* Version: 1.0.1
*
*  CIRCCONV  -  N-point circular convolution of two sequences on the TMS320C50.
*
*  The sequence length N is read from data 0x8000.  x1(n) is copied to a work
*  area; then for each output index the two sequences are multiplied and summed
*  (LT/MPY/APAC-style), the result stored, and x2(n) rotated by one position
*  (subroutine ROTATE, using the ACCB and a BMAR-addressed BLDD to shift the
*  window).  Outputs are written from 0x8010.

.title  "Circular Convolution of Two Sequences"
.mmregs
START:  LDP     #100h
        LACC    0h                      ; N (sequence length) from 0x8000
        SUB     #1h
        SACL    1h                      ; N-1 -> loop count
        LAR     AR0,1h
        LAR     AR1,#8060h
        LAR     AR2,#8100h
*  --- copy x1(n) into the work area ---
COPYX2: MAR     *,AR1
        LACC    *+
        MAR     *,AR2
        SACL    *+
        MAR     *,AR0
        BANZ    COPYX2,*-
        LAR     AR0,1h
        LAR     AR2,#8010h              ; output pointer
*  --- outer loop: one output sample per circular shift ---
LOOP3:  LAR     AR1,#8060h
        LAR     AR3,#8050h
        LAR     AR4,1h
        ZAP
LOOP:   MAR     *,AR3                   ; sum of x1(k)*x2(k)
        LT      *+
        MAR     *,AR1
        MPY     *+
        SPL     5h                      ; partial product
        ADD     5h
        MAR     *,AR4
        BANZ    LOOP,*-
        MAR     *,AR2
        SACL    *+                      ; store output sample
        CALL    ROTATE                  ; rotate x2(n) by one
        MAR     *,AR0
        BANZ    LOOP3,*-
H:      B       H                       ; done: spin
*=============================================================================
*  ROTATE  -  circularly shift the x2(n) window by one position (via ACCB)
*=============================================================================
ROTATE: LDP     #100h
        LACC    1h
        SUB     #1h
        SACL    2h
        LACC    0050h
        SACB                            ; save first element in ACCB
        LAR     AR3,#8051h
        LAR     AR5,#8070h
        LAR     AR6,2h
LOOP1:  MAR     *,AR3
        LACC    *+
        MAR     *,AR5
        SACL    *+
        MAR     *,AR6
        BANZ    LOOP1,*-                ; shift 8051.. -> 8070..
        LACB                            ; recover first element
        MAR     *,AR5
        SACL    *+                      ; append as the last element
        LACC    #8070h
        SAMM    BMAR                    ; BMAR -> rotated buffer
        LAR     AR3,#8050h
        MAR     *,AR3
        RPT     #3h
        BLDD    BMAR,*+                 ; copy rotated window back to 0x8050
        RET
        .end
