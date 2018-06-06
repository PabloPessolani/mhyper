.sect .text
.extern	__execvm
.define	_execvm
.align 2

_execvm:
	jmp	__execvm
