.sect .text
.extern	__vmmcmd
.define	_vmmcmd
.align 2

_vmmcmd:
	jmp	__vmmcmd
