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
*  FIR  -  16-tap finite impulse response filter, Q15 fixed point.
*
*     y[n] = SUM(k=0..15) h[k] * x[n-k]
*
*  The coefficients h[k] live in program memory (HCOEF).  The sample history
*  x[n-15..n] is a data-memory delay line walked by a single repeated MACD:
*  MACD multiplies coefficient by sample, accumulates, and shifts the delay
*  line down one position (DMOV) so the next input simply overwrites the top.
*
*  Entry : AR3 -> oldest sample ; ACC = filtered output (Q15) on exit.

.title  "FIR Filter (16-tap, direct form)"
.mmregs
NTAPS   .set    16
XLINE   .set    060h                    ;delay line base (16 words, page 0)
YOUT    .set    071h                    ;filtered output sample
        .text
FIR     SETC    SXM                     ;signed arithmetic
        SPM     0                       ;no product shift
        LDP     #0                      ;data page 0
        LAR     AR3,#XLINE+NTAPS-1      ;AR3 -> oldest sample x[n-15]
        MAR     *,AR3
        ZAP                             ;ACC = 0, PREG = 0
        RPT     #NTAPS-1                ;16 taps in one instruction
        MACD    HCOEF,*-                ;ACC += h[k]*x ; shift delay line
        APAC                            ;fold in the final product
        SACH    YOUT,1                  ;store Q15 result
        RET
*
*  Filter coefficients h[0..15], Q15, symmetric low-pass (program memory).
*
HCOEF   .word   00123h,00456h,00789h,00a01h,00c34h,00e56h,01078h,01234h
        .word   01234h,01078h,00e56h,00c34h,00a01h,00789h,00456h,00123h
        .end
