.sect .text
.extern	__getsep
.define	_getsep
.align 2

_getsep:
	jmp	__getsep
