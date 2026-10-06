	.version 50
	.text
start:
	LACC	#01234h
	ADD	#1h
	SACL	020h
	B	start
	.data
	.word	0aa55h, 1234h
