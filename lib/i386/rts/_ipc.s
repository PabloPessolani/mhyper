.sect .text; .sect .rom; .sect .data; .sect .bss
.define __echo, __notify, __send, __receive, __sendrec
!MHYPER start
.define __hdebug , __prtmsg
.define __sendas, __recvas, __relay, __notifyas
.define __sendvm, __recvvm, __sendrecvm, __notifyvm
.define __nodebug
!MHYPER end

! See src/kernel/ipc.h for C definitions
SEND = 1
RECEIVE = 2
SENDREC = 3 
NOTIFY = 4
ECHO = 8

!---------------MHYPER start-------------------------------
HDEBUG		=   5
PRTMSG		=   6
SENDAS 		=   7
! ECHO      =   8  
RECVAS		=   9
SENDVM 		=	10   
RECVVM		=	11   
SENDRECVM	=   12   
NOTIFYVM	=   13   
NOTIFYAS	=   14   
RELAY	=   15   
!---------------MHYPER end---------------------------------

SYSVEC = 33			! trap to kernel 

SRC_DST = 8			! source/ destination process 
ECHO_MESS = 8			! echo doesn't have SRC_DST 
MESSAGE = 12			! message pointer 

NTFY_VMID = 12

!MHYPER start
HDEBUG_CODE = 8
HDEBUG_TEXT = 12

PRTMSG_MSRC  = 8
PRTMSG_MPTR  = 12

DST_SRC = 16		
VMID = 16
!MHYPER end



!*========================================================================*
!                           IPC assembly routines			  *
!*========================================================================*
! all message passing routines save ebp, but destroy eax and ecx.
!.define __echo, __notify
!.define __send, __receive, __sendrec 
!.define __hdebug , __prtmsg,
!.define __sendas, __recvas, __relay, __notifyas
!.define __sendvm, __recvvm, __sendrecvm, __notifyvm
.sect .text
__send:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SEND		! _send(dest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__receive:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, RECEIVE		! _receive(src, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__sendrec:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SENDREC		! _sendrec(srcdest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__notify:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! abx = destination 
	mov	ecx, NOTIFY		! _notify(srcdst)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__echo:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	ebx, ECHO_MESS(ebp)	! ebx = message pointer
	mov	ecx, ECHO		! _echo(srcdest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__hdebug:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, HDEBUG_TEXT(ebp)	! eax = text
	mov	ebx, HDEBUG_CODE(ebp)	! ebx = code
	mov	ecx, HDEBUG				! _hdebug(text, code)
	int	SYSVEC					! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__prtmsg:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, PRTMSG_MSRC(ebp)	! eax = msrc
	mov	ebx, PRTMSG_MPTR(ebp)	! ebx = mptr
	mov	ecx, PRTMSG				! _prtmsg(msrc, mptr)
	int	SYSVEC					! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__sendas:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, DST_SRC(ebp)	! edx = src
	mov	eax, SRC_DST(ebp)	! eax = dst
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SENDAS		! _sendas(dst, mptr,src)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret
	

__recvas:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, DST_SRC(ebp)	! edx = dst
	mov	eax, SRC_DST(ebp)	! eax = src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, RECVAS		! _recvas(src, mptr,dst)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret	
  

__relay:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, DST_SRC(ebp)	! edx = src
	mov	eax, SRC_DST(ebp)	! eax = dst
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, RELAY		! _relay(dst, mptr,src)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__notifyas:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, DST_SRC(ebp)	! edx = src
	mov	eax, SRC_DST(ebp)	! eax = dst
	mov	ecx, NOTIFYAS		! _notifyas(dst, src)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__sendvm:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, VMID(ebp)	! edx = vmid
	mov	eax, SRC_DST(ebp)	! eax = dst
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SENDVM		! _sendvm(dst, mptr, vmid)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret
	

__recvvm:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, VMID(ebp)	! edx = dst
	mov	eax, SRC_DST(ebp)	! eax = src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, RECVVM		! _recvvm(src, mptr,vmid)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret	

__sendrecvm:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, VMID(ebp)	! edx = dst
	mov	eax, SRC_DST(ebp)	! eax = src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SENDRECVM		! _sendrecvm(src, mptr,vmid)
	int	SYSVEC		! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__notifyvm:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	edx, NTFY_VMID(ebp)	! edx = vmid
	mov	eax, SRC_DST(ebp)	! eax = destination 
	mov	ecx, NOTIFYVM	! _notifyvm(srcdst,vmid)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__nodebug:
	ret
	
	
	
