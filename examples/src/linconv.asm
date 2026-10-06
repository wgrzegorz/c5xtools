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
*  LINCONV  -  linear convolution y(n) = x(n) * h(n) on the TMS320C50.
*
*  x(n) is in data memory at 0x8100, h(n) at 0x8200, y(n) is written to 0x8300.
*  The impulse response is folded into program memory at 0xC100 with TBLW, x(n)
*  is zero-padded, and each output sample is the sum of products formed by a
*  repeated MACD (multiply / accumulate with data-move) over the taps.  For the
*  demo N1 = N2 = 4, so the result length N1+N2-1 = 7.

.title  "Linear Convolution of Two Sequences"
.mmregs
START:  LDP     #02h
        LAR     AR1,#8100h              ; x(n) data
        LAR     AR0,#8200h              ; h(n) data
        LAR     AR3,#8300h              ; y(n) start
        LAR     AR4,#0007               ; output length N1+N2-1
*  --- fold h(n) into program memory at 0xC100 (reverse order) ---
        LAR     AR0,#08203h
        LACC    #0C100h
        MAR     *,AR0
        RPT     #3
        TBLW    *-
*  --- zero-pad x(n) ---
        LAR     AR6,#8104h
        MAR     *,AR6
        LACC    #0h
        RPT     #3h
        SACL    *+
*  --- convolution: one output sample per outer iteration ---
LOP:    MAR     *,AR1
        LACC    *+
        SACL    050h                    ; current x sample to the MAC window
        LAR     AR2,#0153h              ; end of the window (150h + N1-1)
        MAR     *,AR2
        ZAP
        RPT     #03h                    ; N1 taps
        MACD    0C100h,*-               ; y += h(k)*x ; shift the window
        APAC                            ; accumulate the final product
        MAR     *,AR3
        SACL    *+                      ; store y(n)
        MAR     *,AR4
        BANZ    LOP,*-                  ; next output sample
H:      B       H                       ; done: spin
        .end
