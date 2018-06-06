.sect .text
.extern	__getvmid
.define	_getvmid
.align 2

_getvmid:
	jmp	__getvmid
