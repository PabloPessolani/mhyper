!	memmove()					Author: Kees J. Bot
!								27 Jan 1994
.sect .text; .sect .rom; .sect .data; .sect .bss

! void *memmove(void *s1, const void *s2, size_t n)
!	Copy a chunk of memory.  Handle overlap.
!
.sect .text
.define _memmove
_memmove:
	jmp	__memmove	! Call common code
