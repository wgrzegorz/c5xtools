	.version 50
	.ref extfun
	.text
	.global here
here:	LACC extfun
	B extfun
	CALL extfun
	.end
