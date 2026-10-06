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
*  GOERTZEL  -  single-frequency energy detector (building block of DTMF), Q15.
*
*  Per input sample the second-order recurrence is advanced:
*     s0 = x[n] + coeff*s1 - s2 ;  s2 <- s1 ;  s1 <- s0
*  where coeff = 2*cos(2*pi*k/N).  After BLKLEN samples the tone energy is
*     E = s1^2 + s2^2 - coeff*s1*s2
*  formed with SQRA (square-and-accumulate) and one cross-term multiply.

.title  "Goertzel Single-Tone Detector"
.mmregs
BLKLEN  .set    060h                    ;remaining sample count
COEFF   .set    061h                    ;Goertzel coefficient (Q14)
S0      .set    062h
S1      .set    063h
S2      .set    064h
XIN     .set    065h                    ;current input sample
ENERGY  .set    066h
        .text
GOERTZEL
        SETC    SXM
        SPM     0
        LDP     #0
GZLOOP  LT      S1                      ;T = s1
        MPY     COEFF                   ;P = coeff*s1
        PAC                             ;ACC = P
        ADD     XIN,15                  ;+ x[n]
        SUB     S2,15                   ;- s2
        SACH    S0,1
        LACC    S1
        SACL    S2                      ;s2 <- s1
        LACC    S0
        SACL    S1                      ;s1 <- s0
        LACC    BLKLEN                  ;count down the block
        SUB     #1
        SACL    BLKLEN
        BCND    GZLOOP,GT
        ZAP                             ;energy = s1^2 + s2^2 - coeff*s1*s2
        SQRA    S1
        SQRA    S2
        APAC
        LT      S1
        MPY     S2
        LTP     COEFF
        MPY     S2
        SPAC
        SACH    ENERGY,1
        RET
        .end
