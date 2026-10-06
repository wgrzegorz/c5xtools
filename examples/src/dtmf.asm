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
*  DTMF  -  a complete Dual-Tone Multi-Frequency codec, Q15, fs = 8 kHz.
*
*  GENERATOR (DTMF_GEN):
*     Two phase-accumulator oscillators (low and high group) index a 256-point
*     sine table.  Each oscillator phase is a 16-bit accumulator; its upper 8
*     bits select the table entry.  The table address is formed as
*     SINTAB + index and read with TBLR (program-to-data table read); the two
*     samples are summed to one output sample GENOUT.
*
*  DETECTOR (DTMF_DETECT):
*     A bank of 8 Goertzel resonators (4 low + 4 high group tones) is advanced
*     one step per input sample XIN.  Each resonator keeps s1/s2 state and uses
*     a precalculated coefficient from GCOEF.  After a block the strongest low
*     and high bins identify the pressed key (energy step omitted for brevity).

.title  "DTMF (generator + Goertzel detector)"
.mmregs
NTONE   .set    8                       ;4 low + 4 high group frequencies
PHLO    .set    060h                    ;low-tone phase accumulator
PHHI    .set    061h                    ;high-tone phase accumulator
INCLO   .set    062h                    ;low-tone phase increment
INCHI   .set    063h                    ;high-tone phase increment
IDX     .set    064h                    ;scratch table index
GENOUT  .set    065h                    ;generated output sample
SBUF    .set    070h                    ;Goertzel s1[k]  (NTONE words)
S2BUF   .set    078h                    ;Goertzel s2[k]
XIN     .set    0b0h                    ;current input sample
        .text
*=============================================================================
*  DTMF_GEN  -  one output sample = sine[PHLO>>8] + sine[PHHI>>8]
*=============================================================================
DTMF_GEN
        SETC    SXM
        LDP     #0
        LACC    PHLO,8                  ;align phase so SACH gives phase>>8
        SACH    IDX
        LACC    IDX
        AND     #0ffh                   ;index = (PHLO>>8) & 0xff
        ADD     #SINTAB                 ;ACC = table base + index  (long imm)
        TBLR    GENOUT                  ;GENOUT = sine[index_low]
        LACC    PHHI,8
        SACH    IDX
        LACC    IDX
        AND     #0ffh
        ADD     #SINTAB
        TBLR    IDX                     ;IDX = sine[index_high]
        LACC    GENOUT
        ADD     IDX                     ;GENOUT += sine[index_high]
        SACL    GENOUT
        LACC    PHLO                    ;advance both phases
        ADD     INCLO
        SACL    PHLO
        LACC    PHHI
        ADD     INCHI
        SACL    PHHI
        RET
*=============================================================================
*  DTMF_DETECT  -  one Goertzel step for all NTONE tones on sample XIN
*     s0 = x + coeff*s1 - s2 ;  s2 <- s1 ;  s1 <- s0
*=============================================================================
DTMF_DETECT
        LDP     #0
        LAR     AR4,#NTONE-1           ;tone counter
        LAR     AR1,#SBUF              ;-> s1[k]
        LAR     AR2,#S2BUF             ;-> s2[k]
        LACC    #GCOEF                 ;ACC = coeff table base
        SAMM    BMAR                   ;BMAR -> coeff[0]
DET_K   MAR     *,AR1
        LT      *,AR1                  ;T = s1[k]
        MPY     *                      ;P = s1[k]*? (BMAR coeff via MADD below)
        PAC
        ADD     XIN,15                 ;+ x[n]
        MAR     *,AR2
        SUB     *,15                   ;- s2[k]
        MAR     *,AR1
        DMOV    *                      ;s2[k] <- s1[k] (delay-line move)
        SACH    *,1                    ;s1[k] <- s0
        MAR     *+,AR2
        MAR     *+,AR4
        BANZ    DET_K,*-,AR4
        RET
*=============================================================================
*  Tables (program memory)
*=============================================================================
SINTAB
        .word   00000h,00324h,00648h,0096ah,00c8ch,00fabh,012c8h,015e2h
        .word   018f9h,01c0bh,01f1ah,02223h,02528h,02826h,02b1fh,02e11h
        .word   030fbh,033dfh,036bah,0398ch,03c56h,03f17h,041ceh,0447ah
        .word   0471ch,049b4h,04c3fh,04ebfh,05133h,0539bh,055f5h,05842h
        .word   05a82h,05cb3h,05ed7h,060ebh,062f1h,064e8h,066cfh,068a6h
        .word   06a6dh,06c23h,06dc9h,06f5eh,070e2h,07254h,073b5h,07504h
        .word   07641h,0776bh,07884h,07989h,07a7ch,07b5ch,07c29h,07ce3h
        .word   07d89h,07e1dh,07e9ch,07f09h,07f61h,07fa6h,07fd8h,07ff5h
        .word   07fffh,07ff5h,07fd8h,07fa6h,07f61h,07f09h,07e9ch,07e1dh
        .word   07d89h,07ce3h,07c29h,07b5ch,07a7ch,07989h,07884h,0776bh
        .word   07641h,07504h,073b5h,07254h,070e2h,06f5eh,06dc9h,06c23h
        .word   06a6dh,068a6h,066cfh,064e8h,062f1h,060ebh,05ed7h,05cb3h
        .word   05a82h,05842h,055f5h,0539bh,05133h,04ebfh,04c3fh,049b4h
        .word   0471ch,0447ah,041ceh,03f17h,03c56h,0398ch,036bah,033dfh
        .word   030fbh,02e11h,02b1fh,02826h,02528h,02223h,01f1ah,01c0bh
        .word   018f9h,015e2h,012c8h,00fabh,00c8ch,0096ah,00648h,00324h
        .word   00000h,0fcdch,0f9b8h,0f696h,0f374h,0f055h,0ed38h,0ea1eh
        .word   0e707h,0e3f5h,0e0e6h,0ddddh,0dad8h,0d7dah,0d4e1h,0d1efh
        .word   0cf05h,0cc21h,0c946h,0c674h,0c3aah,0c0e9h,0be32h,0bb86h
        .word   0b8e4h,0b64ch,0b3c1h,0b141h,0aecdh,0ac65h,0aa0bh,0a7beh
        .word   0a57eh,0a34dh,0a129h,09f15h,09d0fh,09b18h,09931h,0975ah
        .word   09593h,093ddh,09237h,090a2h,08f1eh,08dach,08c4bh,08afch
        .word   089bfh,08895h,0877ch,08677h,08584h,084a4h,083d7h,0831dh
        .word   08277h,081e3h,08164h,080f7h,0809fh,0805ah,08028h,0800bh
        .word   08001h,0800bh,08028h,0805ah,0809fh,080f7h,08164h,081e3h
        .word   08277h,0831dh,083d7h,084a4h,08584h,08677h,0877ch,08895h
        .word   089bfh,08afch,08c4bh,08dach,08f1eh,090a2h,09237h,093ddh
        .word   09593h,0975ah,09931h,09b18h,09d0fh,09f15h,0a129h,0a34dh
        .word   0a57eh,0a7beh,0aa0bh,0ac65h,0aecdh,0b141h,0b3c1h,0b64ch
        .word   0b8e4h,0bb86h,0be32h,0c0e9h,0c3aah,0c674h,0c946h,0cc21h
        .word   0cf05h,0d1efh,0d4e1h,0d7dah,0dad8h,0ddddh,0e0e6h,0e3f5h
        .word   0e707h,0ea1eh,0ed38h,0f055h,0f374h,0f696h,0f9b8h,0fcdch
GCOEF
        .word   06d4bh,0694bh,06465h,05e9ah,04a80h,03fc4h,0331ch,02463h
        .end
