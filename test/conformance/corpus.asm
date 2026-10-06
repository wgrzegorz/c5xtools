*====================================================================
* corpus.asm - ISA conformance corpus (committed; do not edit by hand).
* axis A (one representative form per mnemonic) + axis B (operand-form
* matrix). Reloc-free. Refresh the golden with capture.sh; check with
* check.sh.
*====================================================================
	.text
START:
	ABS
	nop
	ADCB
	nop
	ADD 010h
	nop
	ADDB
	nop
	ADDC *+
	nop
	ADDH *+
	nop
	ADDI 010h,2
	nop
	ADDK #5
	nop
	ADDS *+
	nop
	ADDT *+
	nop
	ADLK #0100h,2
	nop
	ADRK #5
	nop
	AND 010h
	nop
	ANDB
	nop
	ANDI *+
	nop
	ANDK #0100h,2
	nop
	APAC
	nop
	APL 010h
	nop
	APLI *+
	nop
	APLK #0100h,010h
	nop
	B 0100h
	nop
	BACC
	nop
	BACCD
	nop
	BANZ 0100h,*-
	nop
	BANZD 0100h,*-
	nop
	BBNZ 0100h
	nop
	BBZ 0100h
	nop
	BC 0100h
	nop
	BCND 0100h,GEQ
	nop
	BCNDD 0100h,GEQ
	nop
	BD 0100h
	nop
	BDDD *+
	nop
	BDSD *+
	nop
	BGEZ 0100h
	nop
	BGZ 0100h
	nop
	BIOZ 0100h
	nop
	BIT 010h,5
	nop
	BITT 010h
	nop
	BKDK 0100h,*+
	nop
	BLDD #0100h,010h
	nop
	BLDP *+
	nop
	BLEZ 0100h
	nop
	BLKD 0100h,010h
	nop
	BLKP 0100h,010h
	nop
	BLPD #0100h,010h
	nop
	BLZ 0100h
	nop
	BNC 0100h
	nop
	BNV 0100h
	nop
	BNZ 0100h
	nop
	BPSD *+
	nop
	BSAR 4
	nop
	BV 0100h
	nop
	BZ 0100h
	nop
	CALA
	nop
	CALAD
	nop
	CALL 0100h
	nop
	CALLD 0100h
	nop
	CC 0100h,GEQ
	nop
	CCD 0100h,GEQ
	nop
	CLRC OVM
	nop
	CMPL
	nop
	CMPR 1
	nop
	CNFD
	nop
	CNFP
	nop
	CPL 010h
	nop
	CPLI *+
	nop
	CPLK #0100h,010h
	nop
	CRGT
	nop
	CRLT
	nop
	DINT
	nop
	DMOV *+
	nop
	EHWBS
	nop
	EHWBT
	nop
	EINT
	nop
	EMPSD
	nop
	EMRET
	nop
	ESTOP
	nop
	ESTPD
	nop
	ETRAP
	nop
	EXAR
	nop
	IDLE
	nop
	IDLE1
	nop
	IDLE2
	nop
	IN 010h,05h
	nop
	INTR 5
	nop
	LAC 010h
	nop
	LACB
	nop
	LACC 010h
	nop
	LACI 010h,2
	nop
	LACK #5
	nop
	LACL 010h
	nop
	LACT *+
	nop
	LALK #0100h,2
	nop
	LAMM *+
	nop
	LAR AR1,#5
	nop
	LARK AR1,#5
	nop
	LARP 1
	nop
	LDP 010h
	nop
	LDPI *+
	nop
	LDPK #1
	nop
	LMMR 010h,#0100h
	nop
	LPH *+
	nop
	LRLK AR1,#0100h
	nop
	LST #0,010h
	nop
	LST0 *+
	nop
	LST1 *+
	nop
	LT *+
	nop
	LTA *+
	nop
	LTD *+
	nop
	LTP *+
	nop
	LTS *+
	nop
	MAC 0100h,*+
	nop
	MACD 0100h,*+
	nop
	MADD *+
	nop
	MADS *+
	nop
	MAR *+
	nop
	MPY 010h
	nop
	MPYA *+
	nop
	MPYI *+
	nop
	MPYK #100
	nop
	MPYS *+
	nop
	MPYU *+
	nop
	MRKL 0100h
	nop
	NEG
	nop
	NMI
	nop
	NOP
	nop
	NORM *+
	nop
	OPL 010h
	nop
	OPLI *+
	nop
	OPLK #0100h,010h
	nop
	OR 010h
	nop
	ORB
	nop
	ORI *+
	nop
	ORK #0100h,2
	nop
	OUT 010h,05h
	nop
	PAC
	nop
	POP
	nop
	POPD *+
	nop
	PSHD *+
	nop
	PUSH
	nop
	RC
	nop
	RET
	nop
	RETC GEQ
	nop
	RETCD GEQ
	nop
	RETD
	nop
	RETE
	nop
	RETI
	nop
	RHM
	nop
	ROL
	nop
	ROLB
	nop
	ROR
	nop
	RORB
	nop
	ROVM
	nop
	RPT 010h
	nop
	RPTB 0100h
	nop
	RPTI *+
	nop
	RPTK #5
	nop
	RPTR 010h
	nop
	RPTZ #5
	nop
	RSXM
	nop
	RTC
	nop
	RXF
	nop
	SACB
	nop
	SACH 010h,1
	nop
	SACL 010h,1
	nop
	SAMM *+
	nop
	SAR AR1,010h
	nop
	SATH
	nop
	SATL
	nop
	SBB
	nop
	SBBB
	nop
	SBLK #0100h,2
	nop
	SBRK #5
	nop
	SC
	nop
	SETC OVM
	nop
	SFL
	nop
	SFLB
	nop
	SFR
	nop
	SFRB
	nop
	SHM
	nop
	SMMR 010h,#0100h
	nop
	SOVM
	nop
	SPAC
	nop
	SPH *+
	nop
	SPL *+
	nop
	SPLK #0100h,010h
	nop
	SPM 1
	nop
	SQRA *+
	nop
	SQRS *+
	nop
	SST #0,010h
	nop
	SST0 *+
	nop
	SST1 *+
	nop
	SSXM
	nop
	STC
	nop
	SUB 010h
	nop
	SUBB *+
	nop
	SUBC *+
	nop
	SUBH *+
	nop
	SUBI 010h,2
	nop
	SUBK #5
	nop
	SUBS *+
	nop
	SUBT *+
	nop
	SWI
	nop
	SXF
	nop
	TBLR *+
	nop
	TBLW *+
	nop
	TEST
	nop
	TRAP
	nop
	TRAPD
	nop
	XC 1,GEQ
	nop
	XOR 010h
	nop
	XORB
	nop
	XORI *+
	nop
	XORK #0100h,2
	nop
	XPL 010h
	nop
	XPLI *+
	nop
	XPLK #0100h,010h
	nop
	ZAC
	nop
	ZALH *+
	nop
	ZALR *+
	nop
	ZALS *+
	nop
	ZAP
	nop
	ZPR
	nop
	LACC 010h
	nop
	LACC 07Fh
	nop
	LACC *
	nop
	LACC *+
	nop
	LACC *-
	nop
	LACC *0+
	nop
	LACC *0-
	nop
	LACC *BR0+
	nop
	LACC *BR0-
	nop
	LACC *+,AR2
	nop
	LACC *,AR3
	nop
	LACC *-,AR0
	nop
	LACC *0+,AR7
	nop
	LACC 010h,4
	nop
	LACC 010h,15
	nop
	LACC *+,4
	nop
	LACC *+,4,AR2
	nop
	LACC *,16
	nop
	LACC #0
	nop
	LACC #1
	nop
	LACC #0FFh
	nop
	LACC #100h
	nop
	LACC #0FFFFh
	nop
	LACC #-1
	nop
	LACC #5,4
	nop
	LACC #0100h,8
	nop
	ADD 010h
	nop
	ADD 010h,4
	nop
	ADD *+
	nop
	ADD *+,4,AR1
	nop
	ADD #0
	nop
	ADD #1
	nop
	ADD #0FFh
	nop
	ADD #100h
	nop
	ADD #0FFFFh
	nop
	ADD #-1
	nop
	ADD #5,9
	nop
	ADD #0FFh,15
	nop
	SUB 010h
	nop
	SUB *+
	nop
	SUB #0FFh
	nop
	SUB #100h
	nop
	SUB #5,4
	nop
	ADDC 010h
	nop
	ADDC *+
	nop
	SUBB 010h
	nop
	ADDS 010h
	nop
	SUBS *+
	nop
	ADDT 010h
	nop
	SUBT *+
	nop
	SUBC 010h
	nop
	AND 010h
	nop
	AND *+
	nop
	AND #0FFh
	nop
	AND #0FFFFh
	nop
	AND #0FFFFh,4
	nop
	OR 010h
	nop
	OR #0FFFFh
	nop
	OR #0FFh,8
	nop
	XOR 010h
	nop
	XOR *+
	nop
	XOR #0FFFFh
	nop
	MPY 010h
	nop
	MPY *+
	nop
	MPY #10
	nop
	MPY #0FFFh
	nop
	MPY #-5
	nop
	MPY #1000h
	nop
	RPT 010h
	nop
	RPT *+
	nop
	RPT #5
	nop
	RPT #0FFh
	nop
	LACL 010h
	nop
	LACL *+
	nop
	LACL #5
	nop
	LACL #0FFh
	nop
	LDP 010h
	nop
	LDP *
	nop
	LDP #5
	nop
	LDP #1FFh
	nop
	BLDD #0100h,010h
	nop
	BLDD #0100h,*+
	nop
	BLDD 010h,#0100h
	nop
	BLDD *+,#0100h
	nop
	BLDD BMAR,010h
	nop
	BLDD BMAR,*+
	nop
	BLDD 010h,BMAR
	nop
	BLDD *+,BMAR
	nop
	BLPD #0100h,010h
	nop
	BLPD #0100h,*+
	nop
	BLPD BMAR,010h
	nop
	BLPD BMAR,*+
	nop
	MAC 0100h,010h
	nop
	MAC 0100h,*+
	nop
	MACD 0100h,010h
	nop
	MACD 0100h,*+
	nop
	SACL 010h
	nop
	SACL 010h,1
	nop
	SACL *+
	nop
	SACL *+,1
	nop
	SACL *+,1,AR2
	nop
	SACH 010h
	nop
	SACH 010h,4
	nop
	SACH *+,4,AR3
	nop
	LAR AR1,010h
	nop
	LAR AR1,*+
	nop
	LAR AR0,#5
	nop
	LAR AR7,#0100h
	nop
	LAR AR3,#0FFh
	nop
	SAR AR1,010h
	nop
	SAR AR2,*+
	nop
	SAR AR0,*0-,AR4
	nop
	LT 010h
	nop
	LT *+
	nop
	LTA 010h
	nop
	LTD *+
	nop
	LTP 010h
	nop
	LTS *+
	nop
	MPYA 010h
	nop
	MPYS *+
	nop
	SPLK #0100h,010h
	nop
	SPLK #0FFFFh,*+
	nop
	LST #0,010h
	nop
	LST #1,010h
	nop
	LST #0,*+
	nop
	SST #0,010h
	nop
	SST #1,*+
	nop
	LMMR 060h,#0100h
	nop
	SMMR 060h,#0100h
	nop
	SMMR 060h,#0FFFFh
	nop
	BIT 010h,0
	nop
	BIT 010h,15
	nop
	BITT 010h
	nop
	BITT *+
	nop
	NORM *
	nop
	NORM *+
	nop
	NORM *-
	nop
	BCND 0100h,GEQ
	nop
	BCND 0100h,LT
	nop
	BCND 0100h,GEQ,C
	nop
	BCND 0100h,GEQ,TC
	nop
	BCNDD 0100h,GT
	nop
	CC 0100h,UNC
	nop
	CC 0100h,EQ
	nop
	CCD 0100h,TC
	nop
	RETC GEQ
	nop
	RETC GEQ,TC
	nop
	RETC UNC
	nop
	XC 1,GEQ
	nop
	XC 2,LEQ
	nop
	XC 1,TC
	nop
	LRLK AR0,#0100h
	nop
	LRLK AR7,#0FFFFh
	nop
	RPTB 0100h
	nop
	RPTZ #5
	nop
	RPTR 010h
	nop
