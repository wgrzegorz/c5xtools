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
*  PROCINIT  -  TMS320C50 processor initialization.
*
*  After reset the routine configures the device: map single-access RAM into
*  data and program space, copy the interrupt/reset vector table to 0x0800,
*  load the vector-table pointer (PMST/IPTR), clear the interrupt-mask and
*  overflow-mode bits, zero the on-chip RAM blocks (B0/B1/B2), then enable
*  interrupts and enter the main program.
*
*  PMST IPTR per device:  C50 081Eh  C51 201Eh  C53 401Eh  C56/C57 801Eh.
*
*  (The interrupt service routines and MAIN_PRG are local stubs here; in a
*  full application they would be separate modules referenced with .ref.)

.title  "Processor Initialization (TMS320C50)"
.mmregs
        .text
*  --- reset / interrupt vector table (copied to 0x0800 by INIT) ---
V_TBL:
RESET   B       INIT                    ; reset -> initialization
INT0    B       ISR0                    ; external interrupt 0
INT1    B       ISR1                    ; external interrupt 1
INT2    B       ISR2                    ; external interrupt 2
INT3    B       ISR3                    ; external interrupt 3
TINT    B       TIME                    ; timer interrupt
RINT    B       RCV                     ; serial-port receive
XINT    B       XMT                     ; serial-port transmit
TRNT    B       TRX                     ; TDM-port receive
TXNT    B       TXMT                    ; TDM-port transmit
INT4    B       ISR4                    ; external interrupt 4
        .space  14*16                   ; reserved (14 words)
TRAP    B       TRP                     ; software trap
NMI     B       NMISR                   ; non-maskable interrupt
*  --- initialization routine ---
INIT    LDP     #0                      ; data page 0
        OPL     #20h,PMST               ; configure S/A RAM in data memory
        LAR     AR7,#0800h              ; data-space address of vector table
        MAR     *,AR7
        RPT     #39
        BLPD    #V_TBL,*+               ; copy 40-word vector table to 0x0800
        SPLK    #0081eh,PMST            ; S/A RAM in program space + IPTR
        SPLK    #01ffh,IMR              ; clear interrupt-mask register
        CLRC    OVM                     ; disable overflow-saturation mode
        LAR     AR7,#60h                ; zero on-chip block B2
        RPTZ    #31
        SACL    *+
        LAR     AR7,#100h               ; zero on-chip block B0
        RPTZ    #511
        SACL    *+
        LAR     AR7,#300h               ; zero on-chip block B1
        RPTZ    #511
        SACL    *+
        CLRC    INTM                    ; globally enable interrupts
        B       MAIN_PRG                ; enter main program
*  --- local stubs (separate modules in a real application) ---
ISR0:
ISR1:
ISR2:
ISR3:
ISR4:
TIME:
RCV:
XMT:
TRX:
TXMT:
TRP:
NMISR:
MAIN_PRG:
stub:   B       stub                    ; placeholder: spin
        .end
