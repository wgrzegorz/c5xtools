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
*  BIQUAD  -  second-order IIR section, Direct Form I, Q15.
*
*     w     = x  - a1*y[n-1] - a2*y[n-2]          (feedback)
*     y[n]  = b0*w + b1*x[n-1] + b2*x[n-2]        (feed-forward)
*
*  Coefficients are 13-bit signed immediates supplied to MPY #k, so the whole
*  section is self-contained (no coefficient table to load).  State variables
*  x1/x2/y1/y2 live on data page 0; the running sum is formed in the product
*  register with LT/MPY/APAC and rounded back to Q15 with SACH ...,1.

.title  "IIR Biquad Section (Direct Form I)"
.mmregs
XIN     .set    060h
X1      .set    061h
X2      .set    062h
Y1      .set    063h
Y2      .set    064h
YOUT    .set    065h
A1      .set    0f300h                  ;-a1   (demo coefficients)
A2      .set    01900h                  ;-a2
B0      .set    00800h
B1      .set    01000h
B2      .set    00800h
        .text
BIQUAD  SETC    SXM
        SPM     0
        LDP     #0
        LACC    XIN,15                  ;ACC = x << 15
        LT      Y1                      ;feedback: - a1*y1 - a2*y2
        MPY     #A1
        APAC
        LT      Y2
        MPY     #A2
        APAC
        SACH    Y2,1                    ;Y2 now holds w (high word)
        ZAP                             ;feed-forward sum
        LT      Y2
        MPY     #B0
        APAC
        LT      X1
        MPY     #B1
        APAC
        LT      X2
        MPY     #B2
        APAC
        SACH    YOUT,1                  ;y[n]
        LACC    X1                      ;shift input history
        SACL    X2
        LACC    XIN
        SACL    X1
        RET
        .end
