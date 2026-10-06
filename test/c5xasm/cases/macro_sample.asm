	.version 50
	.text
; data/text directives (.byte validated separately: the reference assembler lists it byte-granular)
	.long	12345678h
	.string	"AB", "C"
; macro with positional parameters
LOAD2	.macro	src, dst
	LACC	src
	SACL	dst
	.endm
	LOAD2	020h, 021h
	LOAD2	022h, 023h
; substitution symbols + conditional assembly
	.asg	5, COUNT
	.eval	COUNT*2, DBL
	.word	DBL
	.if	COUNT > 3
	ADD	#1h
	.else
	ADD	#2h
	.endif
; repetition
	.loop	3
	NOP
	.endloop
	RET
