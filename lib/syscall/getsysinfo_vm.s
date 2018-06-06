.sect .text
.extern	__getsysinfo_vm
.define	_getsysinfo_vm
.align 2

_getsysinfo_vm:
	jmp	__getsysinfo_vm
