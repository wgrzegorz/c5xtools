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
*  CODEC_LOOPBACK  -  CS4231 stereo-codec digital loopback in PIO mode (Mode 1).
*
*  The codec is configured through its index protocol, then the loop polls the
*  status port and moves one sample at a time: on "capture ready" it reads a
*  16-bit sample (two PIO byte reads) into a circular buffer; on "playback
*  needed" it writes the next buffered sample back (two PIO byte writes). B1 is
*  set up as circular buffer #1 with AR7 as its pointer.

.title  "CS4231 Codec PIO Digital Loopback"
.mmregs
.include "cs4231.inc"
TEMP0   .set    060h
TEMP1   .set    061h
        .text
init:   LDP     #0
        SPLK    #001fh,CWSR             ; program wait-state control
        SPLK    #0fffah,IOWSR           ; 3 wait states on I/O
*  --- bring the codec out of reset and configure it (index protocol) ---
        SPLK    #IDX_PINCTL,TEMP0
        OUT     TEMP0,CS4231_R0         ; select pin-control register
        SPLK    #0000h,TEMP0
        OUT     TEMP0,CS4231_R1         ; drive it to 0 (out of reset)
        SPLK    #IDX_FORMAT,TEMP0
        OUT     TEMP0,CS4231_R0         ; select clock/data format
        SPLK    #FMT_16LIN|FMT_MONO|RATE_48KHZ,TEMP0
        OUT     TEMP0,CS4231_R1         ; 16-bit linear, mono, 48 kHz
        SPLK    #IDX_LADC,TEMP0
        OUT     TEMP0,CS4231_R0         ; select left ADC input
        SPLK    #LADC_MIC|ADC_GAIN2,TEMP0
        OUT     TEMP0,CS4231_R1         ; microphone, +gain
        SPLK    #IDX_CONFIG,TEMP0
        OUT     TEMP0,CS4231_R0         ; select interface configuration
        SPLK    #CFG_CAP|CFG_PLAY,TEMP0
        OUT     TEMP0,CS4231_R1         ; enable capture and playback
*  --- set up B1 as circular buffer #1 (0x8000..0xffff), pointer AR7 ---
        LACC    #08000h
        SACL    CBSR1                   ; buffer start = 0x8000
        LACC    #0ffffh
        SACL    CBER1                   ; buffer end   = 0xffff
        LAR     AR7,#08000h             ; AR7 -> buffer start
        LACC    #11101111b
        SACL    CBCR                    ; enable CB#1 (pointer AR7)
        CLRC    INTM                    ; (interrupts not used in PIO mode)
*=============================================================================
*  main loop: poll status, service capture or playback one sample at a time
*=============================================================================
Loop:   IN      TEMP0,CS4231_R2         ; read status port
        NOP
        LACL    TEMP0
        AND     #STAT_CAP               ; capture sample ready?
        BCND    Capture,NEQ
        LACL    TEMP0
        AND     #STAT_PLY               ; playback sample needed?
        BCND    Playback,NEQ
        B       Loop
*  --- capture: read low then high byte, assemble a 16-bit sample ---
Capture:
        MAR     *,AR7
        IN      *,CS4231_R3             ; low byte -> buffer slot
        LACL    *
        AND     #00ffh
        SACB                            ; stash low byte in ACCB
        IN      *,CS4231_R3             ; high byte
        LACC    *,8                     ; << 8
        SACL    TEMP0
        LACB                            ; recover low byte
        OR      TEMP0                   ; combine high | low
        SACL    *                       ; store assembled sample
        B       Loop
*  --- playback: write low then high byte of the next buffered sample ---
Playback:
        MAR     *,AR7
        LACC    *+                      ; next sample (advance circular ptr)
        SACL    TEMP0
        OUT     TEMP0,CS4231_R3         ; write low byte
        RPT     #07h
        SFR                             ; shift right 8 -> high byte
        SACL    TEMP0
        OUT     TEMP0,CS4231_R3         ; write high byte
        B       Loop
        .end
