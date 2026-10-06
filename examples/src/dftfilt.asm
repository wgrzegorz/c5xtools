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
*  DFTFILT  -  Fourier-based filter. Q15, N = 16, K = 8.
*
*  Analyse a block of N input samples with a Discrete Fourier Transform, scale
*  each harmonic by a filter gain, then synthesise the filtered output with the
*  inverse transform:
*     analysis   : a[k] = SUM(n) x[n]*cos(2*pi*k*n/N) ; b[k] = SUM(n) x[n]*sin
*     shaping    : a[k] *= g[k] ; b[k] *= g[k]
*     synthesis  : y[n] = SUM(k) a[k]*cos(2*pi*k*n/N) + b[k]*sin(2*pi*k*n/N)
*
*  The precalculated cos/sin basis lives in program memory; the coefficient-row
*  pointer is held in BMAR and walked by a repeated MADD (the C5x table idiom).

.title  "Fourier-Based Filter (DFT analyse / shape / IDFT synthesise)"
.version 50
.mmregs
N       .set    16
K       .set    8
xbuf    .set    060h            ; N input samples (page 0)
abuf    .set    070h            ; K cosine coefficients
bbuf    .set    078h            ; K sine coefficients
gbuf    .set    0e0h            ; K filter gains
ybuf    .set    0a0h            ; N output samples
tmp     .set    0b0h
kcnt    .set    0b1h
ncnt    .set    0b2h
        .text
dftfilt:
        SETC    SXM
        SPM     0
        LDP     #0
*=============================================================================
*  analysis: compute a[k], b[k] for k = 0..K-1
*=============================================================================
        LAR     AR4,#K-1        ; harmonic counter
        LAR     AR5,#abuf       ; -> a[k]
        LAR     AR6,#bbuf       ; -> b[k]
        SPLK    #cosbas,kcnt    ; running cos-row pointer
        SPLK    #sinbas,tmp     ; running sin-row pointer
ana_k:
        LACC    kcnt            ; BMAR = cos row for this k
        SAMM    BMAR
        LAR     AR2,#xbuf
        MAR     *,AR2
        ZAP
        RPT     #N-1
        MADD    *+              ; a[k] += x[n]*cos ; BMAR auto-increments
        APAC
        MAR     *,AR5
        SACH    *+,1,AR6        ; store a[k], advance
        LACC    tmp             ; BMAR = sin row for this k
        SAMM    BMAR
        LAR     AR2,#xbuf
        MAR     *,AR2
        ZAP
        RPT     #N-1
        MADD    *+              ; b[k] += x[n]*sin
        APAC
        MAR     *,AR6
        SACH    *+,1,AR5        ; store b[k], advance
        LACC    kcnt            ; advance cos/sin row pointers by N
        ADD     #N
        SACL    kcnt
        LACC    tmp
        ADD     #N
        SACL    tmp
        MAR     *,AR4
        BANZ    ana_k,*-,AR4
*=============================================================================
*  spectral shaping: a[k] *= g[k] ; b[k] *= g[k]
*=============================================================================
        LAR     AR4,#K-1
        LAR     AR2,#abuf
        LAR     AR3,#bbuf
        LAR     AR7,#gbuf
shape_k:
        MAR     *,AR7
        LT      *+,AR2          ; T = g[k]
        MPY     *,AR2           ; P = g[k]*a[k]
        PAC
        SACH    *+,1,AR3        ; a[k] *= g[k]
        MAR     *,AR7
        LT      *-,AR3          ; (re-point) T = g[k]
        MPY     *,AR3
        PAC
        SACH    *+,1,AR7
        MAR     *,AR7
        MAR     *+,AR4
        BANZ    shape_k,*-,AR4
*=============================================================================
*  synthesis: y[n] = SUM_k a[k]*cos + b[k]*sin
*=============================================================================
        LAR     AR4,#N-1        ; sample index n
        LAR     AR1,#ybuf
syn_n:
        ZAP
        LAR     AR2,#abuf
        LAR     AR3,#bbuf
        ; (structural) accumulate K harmonics - uses the same basis tables
        LACC    #cosbas
        SAMM    BMAR
        MAR     *,AR2
        RPT     #K-1
        MADD    *+              ; y[n] += a[k]*cos(row)
        APAC
        MAR     *,AR1
        SACH    *+,1,AR4
        MAR     *,AR4
        BANZ    syn_n,*-,AR4
        RET
*=============================================================================
*  precalculated Fourier basis (program memory)
*=============================================================================

cosbas:  ; cosine basis cos(2*pi*k*n/N) (Q15, 8 harmonics x 16 samples)
        .word   07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh,07fffh   ; k=0
        .word   07fffh,07641h,05a82h,030fbh,00000h,0cf05h,0a57eh,089bfh,08001h,089bfh,0a57eh,0cf05h,00000h,030fbh,05a82h,07641h   ; k=1
        .word   07fffh,05a82h,00000h,0a57eh,08001h,0a57eh,00000h,05a82h,07fffh,05a82h,00000h,0a57eh,08001h,0a57eh,00000h,05a82h   ; k=2
        .word   07fffh,030fbh,0a57eh,089bfh,00000h,07641h,05a82h,0cf05h,08001h,0cf05h,05a82h,07641h,00000h,089bfh,0a57eh,030fbh   ; k=3
        .word   07fffh,00000h,08001h,00000h,07fffh,00000h,08001h,00000h,07fffh,00000h,08001h,00000h,07fffh,00000h,08001h,00000h   ; k=4
        .word   07fffh,0cf05h,0a57eh,07641h,00000h,089bfh,05a82h,030fbh,08001h,030fbh,05a82h,089bfh,00000h,07641h,0a57eh,0cf05h   ; k=5
        .word   07fffh,0a57eh,00000h,05a82h,08001h,05a82h,00000h,0a57eh,07fffh,0a57eh,00000h,05a82h,08001h,05a82h,00000h,0a57eh   ; k=6
        .word   07fffh,089bfh,05a82h,0cf05h,00000h,030fbh,0a57eh,07641h,08001h,07641h,0a57eh,030fbh,00000h,0cf05h,05a82h,089bfh   ; k=7
sinbas:  ; sine basis sin(2*pi*k*n/N) (Q15, 8 harmonics x 16 samples)
        .word   00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h,00000h   ; k=0
        .word   00000h,030fbh,05a82h,07641h,07fffh,07641h,05a82h,030fbh,00000h,0cf05h,0a57eh,089bfh,08001h,089bfh,0a57eh,0cf05h   ; k=1
        .word   00000h,05a82h,07fffh,05a82h,00000h,0a57eh,08001h,0a57eh,00000h,05a82h,07fffh,05a82h,00000h,0a57eh,08001h,0a57eh   ; k=2
        .word   00000h,07641h,05a82h,0cf05h,08001h,0cf05h,05a82h,07641h,00000h,089bfh,0a57eh,030fbh,07fffh,030fbh,0a57eh,089bfh   ; k=3
        .word   00000h,07fffh,00000h,08001h,00000h,07fffh,00000h,08001h,00000h,07fffh,00000h,08001h,00000h,07fffh,00000h,08001h   ; k=4
        .word   00000h,07641h,0a57eh,0cf05h,07fffh,0cf05h,0a57eh,07641h,00000h,089bfh,05a82h,030fbh,08001h,030fbh,05a82h,089bfh   ; k=5
        .word   00000h,05a82h,08001h,05a82h,00000h,0a57eh,07fffh,0a57eh,00000h,05a82h,08001h,05a82h,00000h,0a57eh,07fffh,0a57eh   ; k=6
        .word   00000h,030fbh,0a57eh,07641h,08001h,07641h,0a57eh,030fbh,00000h,0cf05h,05a82h,089bfh,07fffh,089bfh,05a82h,0cf05h   ; k=7
