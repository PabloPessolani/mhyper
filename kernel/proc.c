/* This file contains essentially all of the process and message handling.
 * Together with "mpx.s" it forms the lowest layer of the MINIX kernel.
 * There is one entry point from the outside:
 *
 *   sys_call: 	      a system call, i.e., the kernel is trapped with an INT
 *
 * As well as several entry points used from the interrupt and task level:
 *
 *   lock_notify:     notify a process of a system event
 *   lock_send:	      send a message to a process
 *   lock_enqueue:    put a process on one of the scheduling queues
 *   lock_dequeue:    remove a process from the scheduling queues
 *
 * Changes:
 *   Aug 19, 2005     rewrote scheduling code  (Jorrit N. Herder)
 *   Jul 25, 2005     rewrote system call handling  (Jorrit N. Herder)
 *   May 26, 2005     rewrote message passing functions  (Jorrit N. Herder)
 *   May 24, 2005     new notification system call  (Jorrit N. Herder)
 *   Oct 28, 2004     nonblocking send and receive calls  (Jorrit N. Herder)
 *
 * The code here is critical to make everything work and is important for the
 * overall performance of the system. A large fraction of the code deals with
 * list manipulation. To make this both easy to understand and fast to execute
 * pointer pointers are used throughout the code. Pointer pointers prevent
 * exceptions for the head or tail of a linked list.
 *
 *  node_t *queue, *new_node;	// assume these as global variables
 *  node_t **xpp = &queue; 	// get pointer pointer to head of queue
 *  while (*xpp != NULL) 	// find last pointer of the linked list
 *      xpp = &(*xpp)->next;	// get pointer to next pointer
 *  *xpp = new_node;		// now replace the end (the NULL pointer)
 *  new_node->next = NULL;	// and mark the new end of the list
 *
 * For example, when adding a new node to the end of the list, one normally
 * makes an exception for an empty list and looks up the end of the list for
 * nonempty lists. As shown above, this is not required with pointer pointers.
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/endpoint.h>

#define LANCE_DBG	1

/*
#define IPC_DBG 		1
#define HDEBUG_ENABLE 	1
#define MHDBG 		1
#define LANCE_DBG		1
*/
#include "debug.h"
#include "proc.h"
#include <signal.h>
#include <string.h>

/* Scheduling and message passing functions. The functions are available to
 * other parts of the kernel through lock_...(). The lock temporarily disables
 * interrupts to prevent race conditions.
 */
FORWARD _PROTOTYPE( int MH_send, (struct proc *caller_ptr, int dst_ep, message *m_ptr));
FORWARD _PROTOTYPE( int MH_receive, (struct proc *caller_ptr, int src_ep, message *m_ptr));
FORWARD _PROTOTYPE( int MH_notify, (struct proc *caller_ptr, int dst_p));

FORWARD _PROTOTYPE( int MH_recvas, (struct proc *caller_ptr, int src_ep, message *m_ptr, int dst_ep));
FORWARD _PROTOTYPE( int MH_sendas, (struct proc *caller_ptr, int dst_ep, message *m_ptr, int src_ep));
FORWARD _PROTOTYPE( int MH_ntfyas, (struct proc *caller_ptr, int dst_p, int src_p));
FORWARD _PROTOTYPE( int MH_relay, (struct proc *caller_ptr, int dst_ep, message *m_ptr, int src_ep));

FORWARD _PROTOTYPE( int MH_sendvm, (struct proc *caller_ptr, int dst_ep, message *m_ptr, int dst_vmid));
FORWARD _PROTOTYPE( int MH_recvvm, (struct proc *caller_ptr, int src_ep, message *m_ptr, int src_vmid));
FORWARD _PROTOTYPE( int MH_ntfyvm, (struct proc *caller_ptr, int dst_p, int dst_vmid));

FORWARD _PROTOTYPE( int deadlock, (int function, register struct proc *caller, int src_dst, int dst_vmid,
                                   int prt_flags));
FORWARD _PROTOTYPE( void enqueue, (struct proc *rp));
FORWARD _PROTOTYPE( void dequeue, (struct proc *rp));
FORWARD _PROTOTYPE( void sched, (struct proc *rp, int *queue, int *front));
FORWARD _PROTOTYPE( void pick_proc, (void));

FORWARD _PROTOTYPE( int mini_hdebug, (char *text, int code));
FORWARD _PROTOTYPE( int mini_prtmsg, (int m_source, message *m_ptr));

FORWARD _PROTOTYPE(int notify_proxy, (int driver_id));

#define BuildMess(m_ptr, src, dst_ptr) \
	(m_ptr)->m_source = proc_addr(src)->p_endpoint;		\
	(m_ptr)->m_type = NOTIFY_FROM(src);				\
	(m_ptr)->NOTIFY_TIMESTAMP = get_uptime();			\
	(m_ptr)->NOTIFY_VMID     = proc_addr(src)->p_vmid;	\
	(m_ptr)->NOTIFY_VMBITMAP = priv(dst_ptr)->s_vm_bitmap;	\
	(m_ptr)->NOTIFY_COUNT = priv(dst_ptr)->s_ntfy_cnt[priv(proc_addr(src))->s_id];    \
	switch (src) {							\
	case HARDWARE:							\
		(m_ptr)->NOTIFY_ARG = priv(dst_ptr)->s_int_pending;	\
		if( priv(dst_ptr)->s_ntfy_cnt[priv(hard_ptr)->s_id] == 0) \
			priv(dst_ptr)->s_int_pending = 0;			\
		break;							\
	case SYSTEM:							\
		(m_ptr)->NOTIFY_ARG = priv(dst_ptr)->s_sig_pending;	\
		priv(dst_ptr)->s_sig_pending = 0;			\
		break;							\
	}

#define BuildMess_ptr(m_ptr, src_ptr, dst_ptr) \
	(m_ptr)->m_source = src_ptr->p_endpoint;		\
	(m_ptr)->m_type = NOTIFY_FROM(src_ptr->p_nr);		\
	(m_ptr)->NOTIFY_TIMESTAMP = get_uptime();			\
	(m_ptr)->NOTIFY_VMID     = src_ptr->p_vmid;	\
	(m_ptr)->NOTIFY_VMBITMAP = priv(dst_ptr)->s_vm_bitmap;	\
	(m_ptr)->NOTIFY_COUNT = priv(dst_ptr)->s_ntfy_cnt[priv(src_ptr)->s_id];   \
	switch (src_ptr->p_nr) {							\
	case HARDWARE:							\
		(m_ptr)->NOTIFY_ARG = priv(dst_ptr)->s_int_pending;	\
		if( priv(dst_ptr)->s_ntfy_cnt[priv(hard_ptr)->s_id] == 0) \
			priv(dst_ptr)->s_int_pending = 0;			\
		break;							\
	case SYSTEM:							\
		(m_ptr)->NOTIFY_ARG = priv(dst_ptr)->s_sig_pending;	\
		priv(dst_ptr)->s_sig_pending = 0;			\
		break;							\
	}

#if (CHIP == INTEL)
#define CopyMess(s,sp,sm,dp,dm) \
	cp_mess(proc_addr(s)->p_endpoint, \
		(sp)->p_memmap[D].mem_phys,	\
		(vir_bytes)sm, (dp)->p_memmap[D].mem_phys, (vir_bytes)dm)

#define CopyMess_vm(sn, sv, sp, sm, dp, dm) \
	cp_mess(proc_addr_vm(sv,sn)->p_endpoint, \
		(sp)->p_memmap[D].mem_phys,	\
		(vir_bytes)sm, (dp)->p_memmap[D].mem_phys, (vir_bytes)dm)

#define CopyMess_ep(sep, sp, sm, dp, dm) \
	cp_mess(sep, \
		(sp)->p_memmap[D].mem_phys,	\
		(vir_bytes)sm, (dp)->p_memmap[D].mem_phys, (vir_bytes)dm)

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 does not have cp_mess() in assembly like INTEL. Declare prototype
 * for cp_mess() here and define the function below. Also define CopyMess.
 */
#endif /* (CHIP == M68000) */

int		lance_ep = NONE;
/*===========================================================================*
 *				sys_call				     *
 *===========================================================================*/
PUBLIC int sys_call(call_nr, src_dst_e, m_ptr, bit_map)
int call_nr;			/* system call number and flags */
int src_dst_e;			/* src to receive from or dst to send to */
message *m_ptr;			/* pointer to message in the caller's space */
long bit_map;			/* notification event set or flags */
{
	/* System calls are done by trapping to the kernel with an INT instruction.
	 * The trap is caught and sys_call() is called to send or receive a message
	 * (or both). The caller is always given by 'proc_ptr'.
	 */
	register struct proc *caller_ptr = proc_ptr;	/* get pointer to caller */
	int function = call_nr & SYSCALL_FUNC;	/* get system call function */
	unsigned flags = call_nr & SYSCALL_FLAGS;	/* get flags */
	int mask_entry;				/* bit to check in send mask */
	int group_size;				/* used for deadlock check */
	int result;					/* the system call's result */
	int src_dst, source_e;
	vir_clicks vlo, vhi;		/* virtual clicks containing message to send */
	int dst_src, vmid;
	int check_ptr, check_dest, check_dead, dead_flag;
	static message m, *my_ptr; /* DEBUG */
	struct proc *p_ptr; /* DEBUG */

	check_dead = 0;			/* default: dont check for deadlocks			*/
	check_ptr  = 0;		  	/* default: check message pointer				*/
	check_dest = 0;			/* default: dont check privileges for destinations 	*/

	/*
	* the forth argument could be "vmid" for:
	* sendvm(),recvvm(), sendrecvm(), notifyvm()
	* or could be "dst_src" for sendas(),recvas(), notifyas()
	* or could be "source_e" for relay()
	*/
	vmid = dst_src = source_e = (int) bit_map;

#if LANCE_DBG
	if( lance_ep == NONE) {
		if( strcmp(caller_ptr->p_name, "lance") == 0) {
			lance_ep = caller_ptr->p_endpoint;
		}
	}
	if( (caller_ptr->p_endpoint == lance_ep) || (src_dst_e == lance_ep)  ) {
		my_ptr = &m;
		CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
		MHDEBUG("%s src_dst_e=%d function=%d " MSG1_FORMAT,
		        caller_ptr->p_name, src_dst_e, function, MSG1_FIELDS(my_ptr));
	}
	/*
		else {
			if ( function == NOTIFY ) {
				if( isokendpt(src_dst_e, &src_dst)) {
					p_ptr = proc_addr(src_dst);
					if( strcmp(p_ptr->p_name, "lance") == 0){
						my_ptr = &m;
						CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
						MHDEBUG("%s src_dst_e=%d function=%d" MSG1_FORMAT,
							caller_ptr->p_name, src_dst_e, function, MSG1_FIELDS(my_ptr));
					}
				}
			}
			else {
				if( isokendpt(src_dst_e, &src_dst)){
					p_ptr = proc_addr(src_dst);
					if( strcmp(proc_ptr->p_name, "lance") == 0)
						mini_prtmsg(caller_ptr->p_endpoint, m_ptr);
				}
			}
		}
	*/
#endif /* LANCE_DBG */

	switch(function) {
	case HDEBUG:
	/* prints a debug message (30 chars + 1 integer) */	
#if ENABLE_HDEBUG
		result = mini_hdebug((char *) m_ptr, src_dst_e);
#else
		result = caller_ptr->p_vmid;
#endif
		return(result);
		break;
	case PRTMSG:
	/* prints the contents of a message using FORMAT1 */	
#if MHDBG
		result = mini_prtmsg(src_dst_e, m_ptr);
#else
		result = OK;
#endif
		return(result);
		break;

		/*
		*	ONLY PROXY (ON VM0) PROCESS CAN USE THESE PRIMITIVES
		*/
	case SENDAS:
	case RECVAS:
	case NOTIFYAS:
		/* Verify caller */
		result = OK;
		do {
			/* a proxy must run in VM0 				*/
			if( caller_ptr->p_vmid != VM0) {
				result = EPERM;
				break;
			}

			/* The process must be set as a PROXY 			*/
			if( !(caller_ptr->p_misc_flags & PROXY_SERVER)) {
				result = EPERM;
				break;
			}

			/* The VMID of the VM the proxy represent must be valid */
			if( caller_ptr->p_proxy < VM0 || caller_ptr->p_proxy >= NR_VMS) {
				result = EBADVMID;
				break;
			}
		} while(0);
		if( result) {
			kprintf("ERROR RECVAS/SENDAS/NOTIFYAS: call_nr=%d function=%d result=%d\n"
			        ,call_nr, function,result );
			return(result);
		}
		break;
		/*
		*	ONLY PROMISCUOUS PROCESS (ON VM0) CAN USE THESE PRIMITIVES
		*/
	case SENDVM:
	case RECVVM:
	case SENDRECVM:
	case NOTIFYVM:
		/*	Verify caller 						*/
		result = OK;
		do {
			/* a promiscuous must run in VM0 			*/
			if( caller_ptr->p_vmid != VM0) {
				result = EPERM;
				break;
			}

			/* The VMID must be checked 				*/
			if( function != RECVVM || vmid != ANY_VM) {

				/* The VMID must be valid */
				if( vmid < VM0 || vmid >= NR_VMS) {
					result = EBADVMID;
					break;
				}

				/* Is the VM Loaded and Running			*/
				if ( vmid != VM0 ) {
					if( vm_hyper[vmid].vm_flags != (VM_LOADED | VM_RUNNING) ) {
						result = EVMSTATUS;
						break;
					}
				}

				/* Was the VM recognized by the promiscuous proc?*/
				if( !(priv(caller_ptr)->s_vm_bitmap & (1 << vmid))) {
					result = EBADVMID;
					break;
				}
			}
		} while(0);
		if( result) {
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
			kprintf("SENDVM/RECVVM/NOTIFYVM: call_nr=%d function=%d result=%d"
			        " vmid=%d priv=%X caller_vmid=%d running_vm=%d\n"
			        ,call_nr, function,result, vmid ,
			        priv(caller_ptr)->s_vm_bitmap, caller_ptr->p_vmid, running_vm );
			return(result);
		}
		break;
	case NOTIFY:
	case ECHO:
		break;
	case SENDREC:
	case SEND:
	case RECEIVE:
		break;
	case RELAY:
		MHDEBUG("RELAY caller=%d dst_e=%d src_e=%d " MSG1_FORMAT, caller_ptr->p_endpoint,
		        src_dst_e, source_e, MSG1_FIELDS(my_ptr));
		break;
	default:
		MHDEBUG("ERROR invalid function=%d caller=%d dst_e=%d src_e=%d " MSG1_FORMAT, 
			function, caller_ptr->p_endpoint, src_dst_e, source_e, MSG1_FIELDS(my_ptr));
		return(EBADCALL);
	}

	/* Require a valid source and/ or destination process, unless echoing. */
	if (src_dst_e != ANY && function != ECHO) {
		if( function != RECVVM) {
			if(!isokendpt(src_dst_e, &src_dst)) {
				return EDEADSRCDST;
			}
		} else {
			/* 		RECVVM
				* NOT_PROC flag means: ANY except src_dst
				* the src_dst_e is coded as (src_dst | NOT_FLAG)
				*/
			if( src_dst_e & NOT_PROC ) {
				MHDEBUG("RECVVM: src_dst_e=%d \n",src_dst_e);
				src_dst =  (src_dst_e &	~NOT_PROC);
			} else {
				if(!isokendpt(src_dst_e, &src_dst)) {
					return EDEADSRCDST;
				}
			}
		}
	} else {
		src_dst = src_dst_e;
	}

	/* Check if the process has privileges for the requested call. Calls to the
	* kernel may only be SENDREC, because tasks always reply and may not block
	* if the caller doesn't do receive().
	*/
	if (!(priv(caller_ptr)->s_trap_mask & (1 << function)) ||
	        (iskerneln(src_dst) && function != SENDREC
	         && function != RECEIVE)) {
		MHDEBUG(PRIV_FORMAT,PRIV_FIELDS(priv(caller_ptr)));
		return(ETRAPDENIED);		/* trap denied by mask or kernel */
	}

	/* If the call involves a message buffer, i.e., for SEND, RECEIVE, SENDREC,
	* or ECHO, check the message pointer. This check allows a message to be
	* anywhere in data or stack or gap. It will have to be made more elaborate
	* for machines which don't have the gap mapped.
	*/
	if (function & check_ptr) {
		vlo = (vir_bytes) m_ptr >> CLICK_SHIFT;
		vhi = ((vir_bytes) m_ptr + MESS_SIZE - 1) >> CLICK_SHIFT;
		if (vlo < caller_ptr->p_memmap[D].mem_vir || vlo > vhi ||
		        vhi >= caller_ptr->p_memmap[S].mem_vir +
		        caller_ptr->p_memmap[S].mem_len) {
			return(EFAULT); 		/* invalid message pointer */
		}
	}

	/* If the call is to send to a process, i.e., for SEND, SENDREC or NOTIFY,
	* verify that the caller is allowed to send to the given destination.
	*/
	if (function & check_dest) {
		if (!get_sys_bit(priv(caller_ptr)->s_ipc_to, nr_to_id(src_dst))) {
			return(ECALLDENIED);		/* call denied by ipc mask */
		}
	}

	/* Check for a possible deadlock for blocking SEND(REC) and RECEIVE. */

	dead_flag = FALSE;
	if (check_dead) {
		if (group_size = deadlock(function, caller_ptr, src_dst, vmid, dead_flag)) {
			MHDEBUG("DEADLOCK FOUND!\n");
			dead_flag = TRUE;
			return(ELOCKED); 
		}
	}

	/* Now check if the call is known and try to perform the request. The only
	* system calls that exist in MINIX are sending and receiving messages.
	*   - SENDREC: combines SEND and RECEIVE in a single system call
	*   - SEND:    sender blocks until its message has been delivered
	*   - RECEIVE: receiver blocks until an acceptable message has arrived
	*   - NOTIFY:  nonblocking call; deliver notification or mark pending
	*   - ECHO:    nonblocking call; directly echo back the message
	*   - SENDAS:  A proxy Server sends a message to a requester process on the VM it represents
	*		with a source of a VIRTUAL_TASK on that VM
	*   - RECVAS:  A proxy Server receives a message from a requester process on the VM it represents
	*		with a destination of a VIRTUAL_TASK on that VM
	*   - NOTIFYAS:  A proxy Server sends a notify message to a process on the VM it represents
	*		with a source of a VIRTUAL_TASK on that VM
	*   - SENDVM:  A promiscous task in VM0 sends a message to a process on other VM
	*		with the same source endpoint that it have in VM0
	*   - RECVAS:  A promiscous task receive messages from processes on other VMs
	*   - NOTIFYVM:  A promiscous task in VM0 sends a NOTIFY message to a process on other VM
	*		with the same source endpoint that it have in VM0
	*/
	
	/* reset miscelaneous PENDING flags */
	caller_ptr->p_rts_flags = 0;
	caller_ptr->p_misc_flags &= ~(REPLY_PENDING | VMID_PENDING);
	
	/* Checks for deadlock */
	if(	dead_flag == TRUE) {
		deadlock(function, caller_ptr, src_dst, vmid, dead_flag);
	}
	
	/* WARNING: This switch/case could be improved using a vector of functions */
	switch(function) {
	case SENDREC:
		/* A flag is set so that notifications cannot interrupt SENDREC. */
		caller_ptr->p_misc_flags |= REPLY_PENDING;
#ifdef IPC_DBG
		if(caller_ptr->p_vmid != VM0) {
			my_ptr = &m;
			CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
			MHDEBUG("SENDREC: src=%s dst=%d " MSG1_FORMAT,
			        caller_ptr->p_name, src_dst_e,  MSG1_FIELDS(my_ptr));
		}
#endif /* IPC_DBG */
		/* fall through */
	case SEND:
#ifdef IPC_DBG
		if(caller_ptr->p_vmid != VM0 && function == SEND) {
			my_ptr = &m;
			CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
			MHDEBUG("SEND: src=%s dst=%d " MSG1_FORMAT,
			        caller_ptr->p_name, src_dst_e,  MSG1_FIELDS(my_ptr));
		}
#endif /* IPC_DBG */
		result = MH_send(caller_ptr, src_dst_e, m_ptr);
		if (function == SEND || result != OK) {
			break;			/* done, or SEND failed */
		}				/* fall through for SENDREC */
		/*			caller_ptr->p_misc_flags &= ~(REPLY_PENDING); */
	case RECEIVE:
		result = MH_receive(caller_ptr, src_dst_e, m_ptr);
		break;
	case NOTIFY:
		result = MH_notify(caller_ptr, src_dst);
		break;
	case ECHO:
		/*	CopyMess(caller_ptr->p_nr, caller_ptr, m_ptr, caller_ptr, m_ptr); */
		result = OK;
		break;
	case RELAY:
		result = MH_relay(caller_ptr, src_dst_e, m_ptr, source_e);
		break;
	case SENDAS:
		MHDEBUG("SENDAS: name=%s dst_ep=%d, p_proxy=%d\n",
		        caller_ptr->p_name, src_dst_e, caller_ptr->p_proxy);
		result = MH_sendas(caller_ptr, src_dst_e, m_ptr, dst_src);
		break;
	case RECVAS:
		MHDEBUG("RECVAS: name=%s src_ep=%d, p_proxy=%d dst_src=%d\n",
		        caller_ptr->p_name, src_dst_e, caller_ptr->p_proxy, dst_src);
		result = MH_recvas(caller_ptr, src_dst_e, m_ptr,dst_src);
		break;
	case NOTIFYAS:
		result = MH_ntfyas(caller_ptr, src_dst, dst_src);
		break;
	case SENDRECVM:
		/* A flag is set so that notifications cannot interrupt SENDREC. */
		caller_ptr->p_misc_flags |= REPLY_PENDING;
#ifdef IPC_DBG
		if(running_vm != VM0 ||  vmid != VM0 ) {
			my_ptr = &m;
			CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
			MHDEBUG("SENDRECVM: src=%s dst=%d vmid=%d " MSG1_FORMAT,
			        caller_ptr->p_name, src_dst_e, vmid,  MSG1_FIELDS(my_ptr));
		}
#endif /* IPC_DBG */
		/* fall through */
	case SENDVM:
#ifdef IPC_DBG
		if( (function == SENDVM) && (running_vm != VM0 ||  vmid != VM0) ) {
			my_ptr = &m;
			CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
			MHDEBUG("SENDVM: src=%s dst=%d vmid=%d " MSG1_FORMAT,
			        caller_ptr->p_name, src_dst_e, vmid,  MSG1_FIELDS(my_ptr));
		}
#endif /* IPC_DBG */

		if( caller_ptr->p_vmid == vmid ) {
			result = MH_send(caller_ptr, src_dst_e, m_ptr);
		} else {
			result = MH_sendvm(caller_ptr, src_dst_e, m_ptr, vmid);
		}
		if (function == SENDVM || result != OK) {
			break;			/* done, or SENDVM failed */
		}					/* fall through for SENDRECVM */
	case RECVVM:
		/*	result = MH_recvvm(caller_ptr, src_dst_e, m_ptr, vmid);   */
		if( caller_ptr->p_vmid == vmid ) {
			caller_ptr->p_misc_flags |= VMID_PENDING;
			result = MH_receive(caller_ptr, src_dst_e, m_ptr);
		} else {
			result = MH_recvvm(caller_ptr, src_dst_e, m_ptr,vmid);
		}
		break;
	case NOTIFYVM:
#ifdef IPC_DBG
		if( (running_vm != VM0 ||  vmid != VM0) ) {
			MHDEBUG("NOTIFYVM: src=%s dst=%d vmid=%d \n",
			        caller_ptr->p_name, src_dst_e, vmid);
		}
#endif /* IPC_DBG */

		if( caller_ptr->p_vmid == vmid ) {
			result = MH_notify(caller_ptr, src_dst);
		} else {
			result = MH_ntfyvm(caller_ptr, src_dst, vmid);
		}

		break;
	default:
		result = EBADCALL;		/* illegal system call */
	}


	/* Now, return the result of the system call to the caller. */
	return(result);
}


/*===========================================================================*
 *				deadlock				     *
 *===========================================================================*/
PRIVATE int deadlock(function, cp, src_dst, dst_vmid,  prt_flag)
int function;					/* trap number */
register struct proc *cp;			/* pointer to caller */
int src_dst;					/* src or dst process */
int dst_vmid;						/* vmid  */
int prt_flag;					/* print flag */
{

	register struct proc *xp;			/* process pointer */
	int group_size = 1;				/* start with only caller */
	int trap_flags;
	int vmid;

	if( prt_flag) {
		MHDEBUG("function=%d caller=%s caller_nr=%d caller_vmid=%d src_dst=%d dst_vmid=%d\n",
		        function, cp->p_name, cp->p_nr,  cp->p_vmid, src_dst, dst_vmid);
	}

	while (src_dst != ANY) { 			/* check while process nr */
		int src_dst_e;
		xp = proc_addr(src_dst);		/* follow chain of processes */
		group_size ++;				/* extra process in group */
		if( prt_flag) {
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(xp));
		}

		/* Check whether the last process in the chain has a dependency. If it
		 * has not, the cycle cannot be closed and we are done.
		 */
		if (xp->p_rts_flags & RECEIVING) {	/* xp has dependency */
			if(xp->p_getfrom_e == ANY) {
				src_dst = ANY;
			} else {
				okendpt(xp->p_getfrom_e, &src_dst);
			}
		} else if (xp->p_rts_flags & SENDING) {	/* xp has dependency */
			okendpt(xp->p_sendto_e, &src_dst);
		} else {
			return(0);				/* not a deadlock */
		}

		/* Now check if there is a cyclic dependency. For group sizes of two,
		 * a combination of SEND(REC) and RECEIVE is not fatal. Larger groups
		 * or other combinations indicate a deadlock.
		 */
		if (src_dst == proc_nr(cp)) {		/* possible deadlock */
			if (group_size == 2) {		/* caller and src_dst */
				/* The function number is magically converted to flags. */
				if ((xp->p_rts_flags ^ (function << 2)) & SENDING) {
					return(0);			/* not a deadlock */
				}
			}
			return(group_size);			/* deadlock found */
		}
	}
	return(0);					/* not a deadlock */
}

/*===========================================================================*
 *				MH_send				     						*
 *===========================================================================*/
PRIVATE int MH_send(caller_ptr, dst_ep, m_ptr)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dst_ep;				/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
{
	/* Send a message from 'caller_ptr' to 'dst'. If 'dst' is blocked waiting
	* for this message, copy the message to it and unblock 'dst'. If 'dst' is
	* not waiting at all, or is waiting for another source, queue 'caller_ptr'.
	*/
	register struct proc *dst_ptr , *src_ptr, *dst0_ptr, *dstX_ptr;
	register struct proc **xpp;
	int dst_p, src_p, proxy_ep,proxy_nr ,rcode, call_nr ;
	static message m;
	message *my_ptr; 

	/* Verify destination */
	if( !(rcode = isokendpt(dst_ep, &dst_p)) ) {
		MHDEBUG("MH_send: isokendpt1 caller=%s dst_ep=%d running_vm=%d rcode=%d\n",
		        caller_ptr->p_name, dst_ep, running_vm, rcode);
		MHRETURN(EDSTDIED);
	}
#ifdef TTY_BUG
	else if (caller_ptr == systask_ptr && dst_ep == TTY_PROC_NR) {
		MHDEBUG("MH_send: isokendpt2 caller=%s dst_ep=%d running_vm=%d rcode=%d\n",
		        caller_ptr->p_name, dst_ep, running_vm, rcode);

	}
#endif

#if PRINTER_DBG
	if( caller_ptr->p_endpoint == 35596 && running_vm > VM0)
		MHDEBUG("MH_send: PRINTER dst_ep=%d\n", dst_ep);
#endif /* PRINTER_DBG */

	/* verify if destination is  a valid endpoint */
	if( (caller_ptr->p_misc_flags & PARAVIR_TASK) && (dst_p == SYSTEM)) {
		dst_ptr = &vm_hyper[running_vm].vm_proc[SYSTEM+NR_TASKS];
		MHDEBUG("MH_send: PARAVIR_TASK caller=%s running_vm=%d\n",
			caller_ptr->p_name, running_vm);
		MHASSERT((running_vm > VM0));
	}else{
		dst_ptr = proc_addr(dst_p);
	}
	
	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHDEBUG("MH_send: caller=%s dst_ep=%d dst_p=%d dst_ptr->p_vmid=%d running_vm=%d \n",
		        caller_ptr->p_name, dst_ep, dst_p, dst_ptr->p_vmid, running_vm);
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
		MHRETURN(EDSTDIED);
	}

	/* Caller within VMx */
	if( caller_ptr->p_vmid > VM0)  {

		/* caller within VMx and destination Promiscuous ? */
		if (dst_ptr->p_misc_flags & PROMICUOUS_TASK) {
			/* get the pointer to promiscuous process on VM0 */
			dst0_ptr = dst_ptr->p_promiscuous;
			
#ifdef IPC_DBGxxxxxxx
			/* ONLY FOR DEBBUGGING - por ahora deben estar en la misma ranura */
			MHASSERT( (dst0_ptr == proc_addr_vm(VM0, dst_p)));
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst0_ptr));
#endif /* IPC_DBG */

			/* is the promiscuous process configured to serve that VM? */
			if( (priv(dst0_ptr)->s_vm_bitmap & (1 << caller_ptr->p_vmid))) {
				dstX_ptr = dst_ptr;
				dst_ptr  = dst0_ptr;
			} else {
				MHRETURN(EPROMISCUOUS);
			}

			/* caller within VMx and destination Virtual? */
		} else if (dst_ptr->p_misc_flags &  VIRTUAL_TASK) {
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
#endif /* IPC_DBG */
			/* FOR PARAVIRTUALIZED TASKS: Is SYSTASK the destination ? */
			if( (dst_ptr->p_nr == SYSTEM)) {
				/* Is the source PARAVIRTUALIZED ? */
				if( (caller_ptr->p_misc_flags &  (PARAVIR_TASK | REPLY_PENDING)) 
					== 			(PARAVIR_TASK | REPLY_PENDING)){
					my_ptr = &m;
					CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
					MHDEBUG("src=%s dst=%s " MSG1_FORMAT,
						caller_ptr->p_name, dst_ptr->p_name, MSG1_FIELDS(my_ptr));
						call_nr = (unsigned) my_ptr->m_type - KERNEL_CALL;
					MHDEBUG("MH_send: PARAVIRTUAL caller_nr=%d call_nr=%d s_call2proxy=%X\n", 
					caller_ptr->p_nr, call_nr, priv(caller_ptr)->s_call2proxy );
			
					/* DOES the kernel call be redirected to proxy ? */
					if( !(priv(caller_ptr)->s_call2proxy & (1<<call_nr))){
						/* NO, make a standard send to SYSTASK */
						dstX_ptr = dst_ptr;
						dst_ptr = proc_addr(SYSTEM);
						dst0_ptr= dst_ptr;
					}
				}
			}
			/*  THIS IF SENTENCE IS OK!! does the new dst_ptr be VIRTUAL ? */
			if (dst_ptr->p_misc_flags &  VIRTUAL_TASK) {
				/* get proxy endpoint */
				proxy_ep =  vm_hyper[running_vm].vm_VMM.vmm_endpoint;
				if( !isokendpt_vm(VM0, proxy_ep, &proxy_nr)) {
					MHRETURN(EVIRTUALTASK);
				}

				/* get the pointer to proxy process on VM0 */
				dst0_ptr = proc_addr_vm(VM0, proxy_nr);

				/* is the destination the proxy of the VM? */
				if( dst0_ptr->p_proxy != caller_ptr->p_vmid) {
					MHRETURN(EVIRTUALTASK);
				}
				dstX_ptr = dst_ptr;
			}
			/* caller within VMx and real destination within VMx */
		} else {
			dstX_ptr = dst_ptr;
		}

	} else { /* caller within VM0 and real destination within VM0 */
		dstX_ptr = dst_ptr;
	}
	/* OUTPUT: 									*/
	/* dst_ptr: a pointer to a  real process descriptor			*/
	/* dstX_ptr: a pointer to a fake process descriptor on VMx		*/
	/* dst0_ptr: a pointer to a PROXY or PROMISCUOUS process 	*/
	
	/* Check if 'dst' is blocked waiting for this message. The destination's
	* SENDING flag may be set when its SENDREC call blocked while sending.
	*/
#if LANCE_DBG
	if( dst_ptr->p_endpoint == lance_ep) {
		my_ptr = &m;
		CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, proc_addr(HARDWARE), my_ptr);
		MHDEBUG("src=%s dst=%s " MSG1_FORMAT,
		        caller_ptr->p_name, dst_ptr->p_name, MSG1_FIELDS(my_ptr));
	}
#endif /* LANCE_DBG */

	do {
		/* is destination waiting on receive() or on receiving half of sendrec()  */
		if ( (dst_ptr->p_rts_flags & (RECEIVING | SENDING)) == RECEIVING ) {

			if( dst_ptr->p_misc_flags & VMID_PENDING ) { /* DESTINATION IS PROMISCUOUS */

				/* if caller is within VMX, destination descriptor on VMx  must be promiscuous 	*/
				/* otherwise caller must be on VM0 								*/
				MHASSERT( (	(dstX_ptr->p_misc_flags & PROMICUOUS_TASK) ||
				            (caller_ptr->p_vmid == VM0) ));

				/* Only Promiscuous processes can use receivevm() and NOT_PROC flag, waiting with NOT_GETFROM  misc flag set */
				if( (dst_ptr->p_misc_flags & NOT_GETFROM) &&
				        (dst_ptr->p_getfrom_e == caller_ptr->p_nr)) { /* compare process number, not endpoints */
					MHDEBUG("MH_send NOT_GETFROM MATCH caller_nr=%d dst_ep=%d\n", 
						caller_ptr->p_nr, dst_ptr->p_endpoint);
					break;
				}
				
				/* Compare matching receiver conditions */
				if(	((dst_ptr->p_recv_vm == ANY_VM)  ||				/* wait a message from ANY VM 		*/
				        (dst_ptr->p_recv_vm == caller_ptr->p_vmid)) /* or wait a message from  caller's VM 	*/
				        &&
				        ((dst_ptr->p_getfrom_e == caller_ptr->p_endpoint)  || /* endpoint match ? 		*/
				         (dst_ptr->p_getfrom_e == ANY) ||			/* waiting a message from ANY endpoint 	*/
				         (dst_ptr->p_misc_flags & NOT_GETFROM))) {	/* waiting a message from any endpoint except p_getfrom_e */
					/* Destination is indeed waiting for this message. */
#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if( dst_ptr->p_misc_flags & NOT_GETFROM) {
						MHDEBUG("MH_send caller_nr=%d dst=%d\n", 
						caller_ptr->p_nr, dst_ptr->p_endpoint);
					}
#endif /* IPC_DBG */
					/* copy message from caller user-space to destinations user-space  */
					CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, 
						m_ptr, dst_ptr,dst_ptr->p_messbuf);
					
					/* Return the caller's VMID */
					dst_ptr->p_reg.retreg = caller_ptr->p_vmid;
					
					/* Reset process fields  */
					dst_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM | REPLY_PENDING);
					dst_ptr->p_getfrom_e = NONE;
					dst_ptr->p_recv_vm = ANY_VM;
					
					/* the process has received a message, check if now it is ready */
					if ((dst_ptr->p_rts_flags &= ~RECEIVING) == 0) {
						enqueue(dst_ptr);	/* insert into ready queues */
					}
#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if(caller_ptr->p_vmid > VM0) {
						MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
					}
#endif /* IPC_DBG */
					return(OK);
				}

			} else { /*  DESTINATION IS NOT PROMISCUOUS */
				MHASSERT( (!(dst_ptr->p_misc_flags & NOT_GETFROM)));

				if ( ((dst_ptr->p_vmid == caller_ptr->p_vmid) ||	/* destination an caller on same VM? */
				        (dst_ptr->p_nr == SYSTEM))			/* or destination is SYSTASK  	?*/
				        &&
				        ((dst_ptr->p_getfrom_e == ANY) ||			/* waiting a message from ANY endpoint?	*/
				         (dst_ptr->p_getfrom_e == caller_ptr->p_endpoint))) { /* endpoint match ?		*/

					/* SYSTASK inherits the caller's VMID */
					if( dst_ptr->p_nr == SYSTEM) {
						/* check for paravirtual task */
						systask_ptr->p_vmid = caller_ptr->p_vmid;
					}

					/* Destination is indeed waiting for this message. */
					/* copy message from caller user-space to destinations user-space  */
					CopyMess_ep(caller_ptr->p_endpoint, caller_ptr, m_ptr, dst_ptr,dst_ptr->p_messbuf);
					
					/* Reset process fields  */
					dst_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM | REPLY_PENDING);
					dst_ptr->p_getfrom_e = NONE;
					dst_ptr->p_recv_vm = ANY_VM;

					/* the process has received a message, check if now it is ready */
					if ((dst_ptr->p_rts_flags &= ~RECEIVING) == 0) {
						enqueue(dst_ptr);
					}
#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if(caller_ptr->p_vmid > VM0) {
						MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
					}
#endif /* IPC_DBG */
					return(OK);
				}
			}
		}	
	} while(0);

	/* Destination is not waiting the message.  Block and dequeue caller. */
	caller_ptr->p_messbuf = m_ptr;
	if (caller_ptr->p_rts_flags == 0) {
		dequeue(caller_ptr);
	}
	
	/* set the SENDING flag and the destination endpoint to wait for */
	caller_ptr->p_rts_flags |= SENDING;
	caller_ptr->p_sendto_e = dst_ep;

	/* WARNING: esto se puede mejorar con una doble lista enlazada */
	/* Process is now blocked.  Put in on the destination's queue. */
	xpp = &dst_ptr->p_caller_q;		/* find the end of the list */
	while (*xpp != NIL_PROC) {
		xpp = &(*xpp)->p_q_link;
	}
	*xpp = caller_ptr;			/* add caller to end */
	caller_ptr->p_q_link = NIL_PROC;	/* mark new end of list */

	if( caller_ptr->p_vmid != VM0)  {
		/* if destination is a VIRTUAL TASK with its proxy not receiving, notifies the proxy */
		if (dst_ptr->p_misc_flags &  VIRTUAL_TASK) {
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
#endif /* IPC_DBG */
			if( dst_ptr->p_nr == SYSTEM)	
				MHDEBUG("MH_send: SYSTASK s_id=%d\n", priv(dst_ptr)->s_id);			
			return(notify_proxy(priv(dst_ptr)->s_id));
		}
	}

#if LANCE_DBG
	if( caller_ptr->p_endpoint == lance_ep) {
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
	}
#endif /* LANCE_DBG */

	return(OK);
}


/*===========================================================================*
 *				MH_receive				     *
 *===========================================================================*/
PRIVATE int MH_receive(caller_ptr, src_ep, m_ptr)
register struct proc *caller_ptr;	/* process trying to get message */
int src_ep;				/* which message source is wanted */
message *m_ptr;			/* pointer to message buffer */
{
	/* A process or task wants to get a message.  If a message is already queued,
	 * acquire it and deblock the sender.  If no message from the desired source
	 * is available block the caller, unless the flags don't allow blocking.
	 */
	register struct proc **xpp, *srcX_ptr;
	register struct notification **ntf_q_pp;
	int bit_nr, cond1, cond2, cond0;
	sys_map_t *map;
	bitchunk_t *chunk;
	int i, src_id, src_proc_nr, tmp_ep, src_p;
	register struct proc  *dst_ptr;
	static message m;
	message *my_ptr;
	int vmid, cond;

#ifdef IPC_DBG
	/* ONLY FOR DEBUGGING */
	if(caller_ptr->p_vmid != VM0) {
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
	}
#endif /* IPC_DBG */

	/* assign VM0 to SYSTASK and CLOCK and set running VM */
	if( ((caller_ptr->p_nr == SYSTEM) ||
	        (caller_ptr->p_nr == CLOCK) )  /* HACE FALTA CLOCK ?????????? no deberia ser siempre en VM0 ? */
	        && (src_ep == ANY)) {
		caller_ptr->p_vmid = VM0;
		running_vm = VM0;
	}

	if(src_ep == ANY) {
		src_p = ANY;
	} else {
		if( !isokendpt(src_ep, &src_p)) {
			MHRETURN(ESRCDIED);
		}
		if (proc_addr(src_p)->p_rts_flags & NO_ENDPOINT) {
			MHRETURN(ESRCDIED);
		}
	}

	/* Check to see if a message from desired source is already available.
	* The caller's SENDING flag may be set if SENDREC couldn't send. If it is
	* set, the process should be blocked.
	*/
	if (!(caller_ptr->p_rts_flags & SENDING)) {

		/* Check if there are pending notifications, except for SENDREC. 	*/
		/* NOTIFY messages CAN ONLY BE RECEIVED on processes on VM0 */
		/* or by any ORDINARY process in other VMs				*/
		if (!(caller_ptr->p_misc_flags & REPLY_PENDING) ) {
			map = &priv(caller_ptr)->s_notify_pending;
			for (chunk=&map->chunk[0]; chunk<&map->chunk[NR_SYS_CHUNKS]; chunk++) {
				/* Find a pending notification from the requested source. */
				if (! *chunk) {
					continue;    /* no bits in chunk */
				}
				for (i=0; ! (*chunk & (1<<i)); ++i) {} 	/* look up the bit */
				src_id = (chunk - &map->chunk[0]) * BITCHUNK_BITS + i;
				if (src_id >= NR_SYS_PROCS) {
					break;    /* out of range */
				}
				src_proc_nr = id_to_nr(src_id);	/* get source proc on VMX */
				if (src_ep!=ANY && src_p != src_proc_nr) {
					continue;    /* source not ok */
				}

				if( --priv(caller_ptr)->s_ntfy_cnt[src_id] == 0) {
					*chunk &= ~(1 << i);		/* no longer pending */
				} else {
					MHDEBUG("dst=%s src=%d src_id=%d ntfy_cnt=%d\n",
					        caller_ptr->p_name, src_proc_nr, src_id,
					        priv(caller_ptr)->s_ntfy_cnt[src_id]);
				}

				/* Found a suitable source, deliver the notification message. */
				BuildMess(&m, src_proc_nr, caller_ptr);	/* assemble message */
				CopyMess(src_proc_nr, proc_addr(HARDWARE), &m, caller_ptr, m_ptr);

				return(OK);					/* report success */
			}
		}

		/* Check caller queue. Use pointer pointers to keep code simple. */
		xpp = &caller_ptr->p_caller_q;

		while (*xpp != NIL_PROC) {
			cond1 = (caller_ptr->p_vmid == (*xpp)->p_vmid)?TRUE:FALSE;
			cond1 |= (caller_ptr->p_nr == SYSTEM)?TRUE:FALSE;
			cond0 = ( 	caller_ptr->p_vmid != VM0 &&
						(*xpp)->p_vmid == VM0 	&&
						((*xpp)->p_misc_flags & VMID_PENDING) &&
						((*xpp)->p_rts_flags & (RECEIVING|SENDING)) == (RECEIVING|SENDING))?TRUE:FALSE;
			cond1 |= cond0;			
			cond2 = (src_ep == ANY)?TRUE:FALSE;
			if( cond0) {
				srcX_ptr = proc_addr(priv((*xpp))->s_nr[caller_ptr->p_vmid]);
				cond2 |= (src_ep == srcX_ptr->p_endpoint)?TRUE:FALSE;
			}else{
				cond2 |= (src_ep == (*xpp)->p_endpoint)?TRUE:FALSE;
			}
			if( cond1 & cond2 ) {
				/* Found acceptable message. Copy it and update status. */
				if( caller_ptr->p_nr == SYSTEM )  {
					running_vm = (*xpp)->p_vmid;
					systask_ptr->p_vmid = (*xpp)->p_vmid;
				}

				if( (*xpp)->p_rts_flags & NOTIFYING) {	/* is it a NOTIFY message from VMx */
					/* Found a suitable source, deliver the notification message. */
					BuildMess_ptr(&m, (*xpp), caller_ptr); /* assemble message */
					CopyMess_ep((*xpp)->p_endpoint, hard_ptr, &m, caller_ptr, m_ptr);
					if (((*xpp)->p_rts_flags &= ~NOTIFYING) == 0) {
						enqueue(*xpp);
					}
				} else {
					if(cond0) {
						CopyMess_ep(srcX_ptr->p_endpoint,
							(*xpp), (*xpp)->p_messbuf, 
							caller_ptr, m_ptr);	
						MHDEBUG("src_ep=%d srcX_ep=%d\n",
								(*xpp)->p_endpoint, 
								srcX_ptr->p_endpoint);
					}else{
						CopyMess_ep((*xpp)->p_endpoint, 
							*xpp, (*xpp)->p_messbuf,
							caller_ptr, m_ptr);
					}
				
					if (((*xpp)->p_rts_flags &= ~SENDING) == 0) {
						enqueue(*xpp);
					}
				}
				(*xpp)->p_sendto_e = NONE;
				*xpp = (*xpp)->p_q_link;	/* remove from queue */
				caller_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM | REPLY_PENDING);
				if ((*xpp)->p_rts_flags == 0) {
					enqueue(*xpp);
				}
				return(OK);					/* report success */
			}
			xpp = &(*xpp)->p_q_link;		/* proceed to next */
		}
	}
	/* No suitable message is available or the caller couldn't send in SENDREC.
	* Block the process trying to receive, unless the flags tell otherwise.
	*/
	/* if ( flags & NON_BLOCKING )  return(ENOTREADY); */


	caller_ptr->p_getfrom_e = src_ep;
	caller_ptr->p_messbuf = m_ptr;
	if (caller_ptr->p_rts_flags == 0) {
		dequeue(caller_ptr);
	}
	caller_ptr->p_rts_flags |= RECEIVING;
	caller_ptr->p_recv_vm = caller_ptr->p_vmid;

#if LANCE_DBG
	if( caller_ptr->p_endpoint == lance_ep) {
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
	}
#endif /* LANCE_DBG */
	
	return(OK);
}

/*===========================================================================*
 *				MH_notify				     *
 *===========================================================================*/
PRIVATE int MH_notify(caller_ptr, dst_p)
register struct proc *caller_ptr;	/* sender of the notification */
int dst_p;				/* which process to notify */
{
	register struct proc **xpp;
	register struct proc *dst_ptr, *dst0_ptr, *dstX_ptr;
	int src_id, proxy_ep,proxy_nr;
	static message m;

	dst_ptr = proc_addr(dst_p);
	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(EDSTDIED);
	}

#if LANCE_DBG
	if( (caller_ptr->p_nr == RS_PROC_NR) && (dst_ptr->p_endpoint == lance_ep)) {
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
	}
#endif /* LANCE_DBG */

	if( caller_ptr->p_vmid != VM0)  {

		/* caller within VMx and destination Promiscuous ? */
		if (dst_ptr->p_misc_flags & PROMICUOUS_TASK) {
			dst0_ptr = dst_ptr->p_promiscuous;
			MHASSERT( (dst0_ptr == proc_addr_vm(VM0, dst_p)));
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst0_ptr));
#endif /* IPC_DBG */

			/* is the promiscuous process configured to serve that VM? */
			if( (priv(dst0_ptr)->s_vm_bitmap & (1 << caller_ptr->p_vmid))) {
				dstX_ptr = dst_ptr;
				dst_ptr  = dst0_ptr;
				MHASSERT(dst0_ptr->p_misc_flags & VMID_PENDING);
			} else {
				MHRETURN(EPROMISCUOUS);
			}

			/* caller within VMx and destination Virtual? */
		} else if (dst_ptr->p_misc_flags &  VIRTUAL_TASK) {
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
#endif /* IPC_DBG */
			proxy_ep =  vm_hyper[running_vm].vm_VMM.vmm_endpoint;
			if( !isokendpt_vm(VM0, proxy_ep, &proxy_nr)) {
				MHRETURN(EVIRTUALTASK);
			}
			dst0_ptr = proc_addr_vm(VM0, proxy_nr);

			/* is the destination the proxy of the VM? */
			if( dst0_ptr->p_proxy != caller_ptr->p_vmid) {
				MHRETURN(EVIRTUALTASK);
			}
			dstX_ptr = dst_ptr;

			/* caller within VMx and destination real within VMx */
		} else {
			dstX_ptr = dst_ptr;
		}

	} else { /* caller within VM0 and destination real within VM0 */
		dstX_ptr = dst_ptr;
	}


	/* Check to see if target is blocked waiting for this message. A process
	* can be both sending and receiving during a SENDREC system call.
	*/
	do {
		if ((dst_ptr->p_rts_flags & (RECEIVING|SENDING)) == RECEIVING &&
		        !(dst_ptr->p_misc_flags & REPLY_PENDING)) {

#if LANCE_DBG
			if( (caller_ptr->p_nr == RS_PROC_NR) && (dst_ptr->p_endpoint == lance_ep)) {
				MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
			}
#endif /* LANCE_DBG */

			if (dst_ptr->p_misc_flags & VMID_PENDING) { /*Destination is a PROMICUOUS_TASK doing RECVVM() */

				MHASSERT( (	(dstX_ptr->p_misc_flags & PROMICUOUS_TASK) ||
				            (caller_ptr->p_vmid == VM0) ));

				if( (dst_ptr->p_misc_flags & NOT_GETFROM) &&
				        (dst_ptr->p_getfrom_e == caller_ptr->p_nr)) { /* compare process number, not endpoints */
					MHDEBUG("MH_notify MATCH caller_nr=%d dst=%d\n", caller_ptr->p_nr, dst_ptr->p_endpoint);
					break;
				}

				if(	((dst_ptr->p_recv_vm == ANY_VM)  ||
				        (dst_ptr->p_recv_vm == caller_ptr->p_vmid) ||
				        (caller_ptr->p_nr == HARDWARE) ) 	/* HARDWARE is the EXCEPTION */
				        &&
				        ((dst_ptr->p_getfrom_e == caller_ptr->p_endpoint)  ||
				         (dst_ptr->p_getfrom_e == ANY) ||
				         (dst_ptr->p_misc_flags & NOT_GETFROM))) {
					/* Destination is indeed waiting for a message. Assemble a notification
					* message and deliver it. Copy from pseudo-source HARDWARE, since the
					* message is in the kernel's address space.
					*/

					if( dst_ptr->p_misc_flags & NOT_GETFROM) {
						MHDEBUG("MH_notify caller_nr=%d dst=%d flags=%X\n",
						        caller_ptr->p_nr, dst_ptr->p_endpoint, dst_ptr->p_rts_flags);
					}

					/* because Build message delete it */
					BuildMess_ptr(&m, caller_ptr, dst_ptr);
					CopyMess_ep(caller_ptr->p_endpoint, hard_ptr, &m, dst_ptr, dst_ptr->p_messbuf);

					if (caller_ptr->p_nr == HARDWARE) {
						if (dst_ptr->p_recv_vm == ANY_VM) {
							dst_ptr->p_reg.retreg = VM0;
						} else {
							dst_ptr->p_reg.retreg = dst_ptr->p_recv_vm;
						}
					} else {
						dst_ptr->p_reg.retreg = caller_ptr->p_vmid;
					}
					dst_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM);
					dst_ptr->p_recv_vm = ANY_VM;
					dst_ptr->p_getfrom_e = NONE;
					dst_ptr->p_rts_flags &= ~RECEIVING;	/* deblock destination */
					if (dst_ptr->p_rts_flags == 0) {
						enqueue(dst_ptr);
					}
#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if( caller_ptr->p_vmid > VM0) {
						MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
					}
#endif /* IPC_DBG */
					return(OK);
				}

			} else {

				MHASSERT( (!(dst_ptr->p_misc_flags & NOT_GETFROM)));
				/* Destination is not ready to receive the notification */

				if( (caller_ptr->p_vmid != VM0) &&   /*Destination is a PROMICUOUS_TASK doing RECEIVE() */
				        (dstX_ptr->p_misc_flags & PROMICUOUS_TASK)) {
					break;
				}


				if ( (dst_ptr->p_getfrom_e == ANY) ||
				        (dst_ptr->p_getfrom_e == caller_ptr->p_endpoint)) {
					/* Destination is indeed waiting for a message. Assemble a notification
					* message and deliver it. Copy from pseudo-source HARDWARE, since the
					* message is in the kernel's address space.
					*/
					if( dst_ptr->p_nr == SYSTEM) {
						systask_ptr->p_vmid = caller_ptr->p_vmid;
					}
					BuildMess_ptr(&m, caller_ptr, dst_ptr);
					CopyMess_ep(caller_ptr->p_endpoint, hard_ptr, &m,
					            dst_ptr, dst_ptr->p_messbuf);

					dst_ptr->p_getfrom_e = NONE;
					dst_ptr->p_rts_flags &= ~RECEIVING;	/* deblock destination */
					if (dst_ptr->p_rts_flags == 0) {
						enqueue(dst_ptr);
					}
					return(OK);
				}
			}
		}
	} while(0);

	/* Destination is not ready to receive the notification */
	if( caller_ptr->p_vmid != VM0) {
		if( (dstX_ptr->p_misc_flags & PROMICUOUS_TASK) ||
		        (dstX_ptr->p_misc_flags & VIRTUAL_TASK))	 {

			/* Block and dequeue caller. */
			dequeue(caller_ptr);
			caller_ptr->p_rts_flags |= NOTIFYING;

			/* Process is now blocked.  Put in on the destination's queue. */
			if( dstX_ptr->p_misc_flags & PROMICUOUS_TASK) {
				xpp = &dst0_ptr->p_caller_q;    /* find end of list */
			} else {
				xpp = &dstX_ptr->p_caller_q;    /* find end of list */
			}

			while (*xpp != NIL_PROC) {
				xpp = &(*xpp)->p_q_link;
			}
			*xpp = caller_ptr;			/* add caller to end */
			caller_ptr->p_q_link = NIL_PROC;	/* mark new end of list */
			caller_ptr->p_sendto_e   = dstX_ptr->p_endpoint;
			if( dstX_ptr->p_misc_flags & VIRTUAL_TASK) {
				return(notify_proxy(priv(dst0_ptr)->s_id));
			}
			return(OK);
		}
	}

	/* ONLY notifications from VM0 set the notifications bits */
	/* Destination is not ready to receive the notification. Add it to the
	 * bit map with pending notifications. Note the indirectness: the system id
	* instead of the process number is used in the pending bit map.
	*/
#if LANCE_DBG
	if( (caller_ptr->p_nr == RS_PROC_NR) && (dst_ptr->p_endpoint == lance_ep)) {
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
	}
#endif /* LANCE_DBG */
	src_id = priv(caller_ptr)->s_id;
	set_sys_bit(priv(dst_ptr)->s_notify_pending, src_id);
	priv(dst_ptr)->s_ntfy_cnt[src_id]++;
	/*	if( priv(dst_ptr)->s_ntfy_cnt[src_id] != 1) */
	MHDEBUG("MH_notify dst=%s src=%s src_id=%d ntfy_cnt=%d\n",
	        dst_ptr->p_name, caller_ptr->p_name, src_id,
	        priv(dst_ptr)->s_ntfy_cnt[src_id] );
	return(OK);
}


/*===========================================================================*
 *				MH_sendas				     						*
 *===========================================================================*/
PRIVATE int MH_sendas(caller_ptr, dst_ep, m_ptr, src_ep)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dst_ep;				/* to whom is message being sent? */
message *m_ptr;				/* pointer to message buffer */
int src_ep;				/* source of the message */
{
	/* Send a message from 'caller_ptr' to 'dst'. If 'dst' is blocked waiting
	* for this message, copy the message to it and unblock 'dst'. If 'dst' is
	* not waiting at all, or is waiting for another source, queue 'caller_ptr'.
	*/
	register struct proc *dst_ptr , *src_ptr, *dst0_ptr, *dstX_ptr;
	register struct proc **xpp;
	int dst_p, src_p;
	int dst_vmid;

#ifdef IPC_DBG
	/* ONLY FOR DEBUGGING */
	MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif /* IPC_DBG */

	dst_vmid = caller_ptr->p_proxy;		/* the ID of the VM that the proxy represents */

	if( !isokendpt_vm(dst_vmid, dst_ep, &dst_p)) {
		MHRETURN(EDSTDIED);
	}
	dst_ptr = proc_addr_vm(dst_vmid, dst_p);

	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(EDSTDIED);
	}

	if (	(dst_ptr->p_misc_flags & VIRTUAL_TASK) ||
	        (dst_ptr->p_misc_flags & PROMICUOUS_TASK)) {
		MHRETURN(EBADSRCDST);
	}

	src_p = src_ep;	/* proxies emulate process numbers, not endpoints */
	if( dst_p == SYSTEM)
		src_ptr = &vm_hyper[dst_vmid].vm_proc[SYSTEM+NR_TASKS];
	else
		src_ptr = proc_addr_vm(dst_vmid, src_p);

	src_ep = src_ptr->p_endpoint;
	if(!(src_ptr->p_misc_flags & VIRTUAL_TASK)) {
		MHRETURN(EVIRTUALTASK);
	}
	if (!(dst_ptr->p_rts_flags & RECEIVING)) {
		MHRETURN(EPROCSTATUS);
	}
	if ( (dst_ptr->p_rts_flags & SENDING)) {
		MHRETURN(EPROCSTATUS);
	}
	if(  (dst_ptr->p_getfrom_e != src_ptr->p_endpoint) &&
	        (dst_ptr->p_getfrom_e != ANY)) {
		MHRETURN(ESRCDIED);
	}

	/* Destination is indeed waiting for this message. */
	dst_ptr->p_getfrom_e = NONE;
	CopyMess_ep(src_ptr->p_endpoint, caller_ptr, m_ptr, dst_ptr,
	            dst_ptr->p_messbuf);
	if ((dst_ptr->p_rts_flags &= ~RECEIVING) == 0) {
		enqueue(dst_ptr);
	}

	return(OK);
}

/*===========================================================================*
 *				MH_recvas				     *
 *===========================================================================*/
PRIVATE int MH_recvas(caller_ptr, src_ep, m_ptr, dst_ep)
register struct proc *caller_ptr;	/* process trying to get message */
int src_ep;				/* which message source is wanted */
message *m_ptr;			/* pointer to message buffer */
int dst_ep;				/* destination of the message */
{
	register struct proc **xpp, *callerX_ptr;
	register struct notification **ntf_q_pp;
	int bit_nr;
	sys_map_t *map;
	bitchunk_t *chunk;
	int i, src_id, src_proc_nr, src_p, dst_p;
	register struct proc  *dst_ptr;
	static message m;
	message *my_ptr;
	int vmid, cond, src_vmid;

#ifdef IPC_DBG
	/* ONLY FOR DEBUGGING */
	MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif /* IPC_DBG */

	src_vmid = caller_ptr->p_proxy;		/* the ID of the VM that the proxy represents */

	dst_p = dst_ep;						/*  proxy emulate a process number, not an endpoint */
	if( dst_p == SYSTEM)
		dst_ptr = &vm_hyper[src_vmid].vm_proc[SYSTEM+NR_TASKS];
	else
		dst_ptr = proc_addr_vm(src_vmid, dst_p);
	if( dst_ptr == NULL) {
		MHRETURN(EDSTDIED);
	}
	if( !(dst_ptr->p_misc_flags & VIRTUAL_TASK)) {
		MHRETURN(EVIRTUALTASK);
	}
	dst_ep = dst_ptr->p_endpoint;

	if(src_ep == ANY) {
		src_p = ANY;
	} else {
		if( !isokendpt_vm(src_vmid, src_ep, &src_p)) {
			MHRETURN(ESRCDIED);
		}
		if (proc_addr_vm(src_vmid, src_p)->p_rts_flags & NO_ENDPOINT) {
			MHRETURN(ESRCDIED);
		}
	}

	/*
	* A message from desired source must be available.
	* because it was informed before by a NOTIFY message to the proxy
	* Otherwise, it must return an error
	*/


	/* get the bitmap from the VIRTUAL driver */
	map = &priv(dst_ptr)->s_notify_pending;
	for (chunk=&map->chunk[0]; chunk<&map->chunk[NR_SYS_CHUNKS]; chunk++) {
		/* Find a pending notification from the requested source. */
		if (! *chunk) {
			continue;    /* no bits in chunk */
		}
		for (i=0; ! (*chunk & (1<<i)); ++i) {} 	/* look up the bit */
		src_id = (chunk - &map->chunk[0]) * BITCHUNK_BITS + i;
		if (src_id >= NR_SYS_PROCS) {
			break;    /* out of range */
		}
		src_proc_nr = id_to_nr(src_id);		/* get source proc */

		if (src_ep!=ANY && src_p != src_proc_nr) {
			continue;    /* source not ok */
		}
		if( --priv(dst_ptr)->s_ntfy_cnt[src_id] == 0) {
			*chunk &= ~(1 << i);    /* no longer pending */
		} else
			MHDEBUG("dst=%s src=%d src_id=%d ntfy_cnt=%d\n",
			        dst_ptr->p_name, src_proc_nr, src_id,
			        priv(dst_ptr)->s_ntfy_cnt[src_id]);
		/* Found a suitable source, deliver the notification message. */
		BuildMess(&m, src_proc_nr, caller_ptr);	/* assemble message */
		CopyMess(src_proc_nr, hard_ptr, &m, caller_ptr, m_ptr);
		return(OK);					/* report success */
	}


	xpp = &dst_ptr->p_caller_q;
	while (*xpp != NIL_PROC) {
		if ( src_ep == ANY || src_ep == (*xpp)->p_endpoint) {
			/* Found acceptable message. Copy it and update status. */

			vmid = (*xpp)->p_vmid;
#ifdef IPC_DBG
			/****************** FOR DEBUGGING ************************/
			if( vmid != VM0) {
				my_ptr = &m;
				CopyMess_ep((*xpp)->p_endpoint, *xpp, (*xpp)->p_messbuf, proc_addr(HARDWARE), my_ptr);
				/* ONLY FOR DEBUGGING */
				MHDEBUG("MH_recvas: src=%s dst=%s misc=%X vmid=%d " MSG1_FORMAT,
				        (*xpp)->p_name, caller_ptr->p_name, caller_ptr->p_misc_flags, vmid
				        ,MSG1_FIELDS(my_ptr));
			}
#endif /* IPC_DBG */

			CopyMess_ep((*xpp)->p_endpoint, *xpp, (*xpp)->p_messbuf, caller_ptr, m_ptr);
			if ( ((*xpp)->p_rts_flags &= ~(SENDING | NOTIFYING)) == 0) {
				enqueue(*xpp);
			}
			(*xpp)->p_sendto_e = NONE;
			*xpp = (*xpp)->p_q_link;		/* remove from queue */
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif /* IPC_DBG */
			return(OK);
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
	}



	/* PROXY_SERVER does not wait */
	return(EAGAIN);
}

/*===========================================================================*
 *				MH_ntfyas				     *
 * Situation: a process inside VMx (RS) and send a SEND message to a driver	*
 * represented by the proxy.Then RS waits for a notification message from the *
 * driver as a keep alive message						*
 * When the RS send its message to a virtual driver, it sleep until the proxy *
 * makes the recvas().
 *===========================================================================*/
PRIVATE int MH_ntfyas(caller_ptr, dst_p, src_p)
register struct proc *caller_ptr;	/* sender of the notification */
int dst_p;				/* which process to notify */
int src_p;				/* as which source process emulates */
{
	register struct proc *dst_ptr, *src_ptr;
	int src_id, src_ep;
	static message m;
	int dst_vmid;

#ifdef IPC_DBG
	/* ONLY FOR DEBUGGING */
	MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif /* IPC_DBG */

	dst_vmid = caller_ptr->p_proxy;		/* the ID of the VM that the proxy represents */
	dst_ptr = proc_addr_vm(dst_vmid, dst_p);
	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(EDSTDIED);
	}
	if (	(dst_ptr->p_misc_flags & VIRTUAL_TASK) ||
	        (dst_ptr->p_misc_flags & PROMICUOUS_TASK)) {
		MHRETURN(EBADSRCDST);
	}

	src_ptr = proc_addr_vm(dst_vmid, src_p);
	src_ep = src_ptr->p_endpoint;
	if(!(src_ptr->p_misc_flags & VIRTUAL_TASK)) {
		MHRETURN(EVIRTUALTASK);
	}


	/* Check to see if target is blocked waiting for this message. A process
	* can be both sending and receiving during a SENDREC system call.
	*/
	if ((dst_ptr->p_rts_flags & (RECEIVING|SENDING)) == RECEIVING &&
	        ! (dst_ptr->p_misc_flags & REPLY_PENDING)) {
		if ( (dst_ptr->p_getfrom_e == ANY) ||
		        (dst_ptr->p_getfrom_e == src_ptr->p_endpoint)) {
			BuildMess_ptr(&m, src_ptr, dst_ptr);
			CopyMess_ep(src_ptr->p_endpoint, proc_addr(HARDWARE), &m,
			            dst_ptr, dst_ptr->p_messbuf);
			dst_ptr->p_getfrom_e = NONE;
			dst_ptr->p_recv_vm = ANY_VM;
			dst_ptr->p_rts_flags &= ~RECEIVING;	/* deblock destination */
			if (dst_ptr->p_rts_flags == 0) {
				enqueue(dst_ptr);
			}
			return(OK);
		}
	}

	src_id = priv(src_ptr)->s_id;
	set_sys_bit(priv(dst_ptr)->s_notify_pending, src_id);
	priv(dst_ptr)->s_ntfy_cnt[src_id]++;
	/*	if( priv(dst_ptr)->s_ntfy_cnt[src_id] != 1) */
	MHDEBUG("dst=%s src=%s src_id=%d ntfy_cnt=%d\n",
	        dst_ptr->p_name, src_ptr->p_name, src_id,
	        priv(dst_ptr)->s_ntfy_cnt[src_id] );
	return(OK);
}

/*===========================================================================*
 *				MH_sendvm				     						*
 *===========================================================================*/
PRIVATE int MH_sendvm(caller_ptr, dst_ep, m_ptr, dst_vmid)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dst_ep;				/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
int dst_vmid;				/* destination vmid */
{
	/* Send a message from 'caller_ptr' on VM0 to dst_ep on dst_vmid
	* on behalf of src_ep on dst_vmid (dst_vmid != VM0)
	*/
	register struct proc *dst_ptr, *callerX_ptr;
	register struct proc **xpp;
	int dst_p;

	/* Verify correct Destination  */
	if( !isokendpt_vm(dst_vmid, dst_ep, &dst_p)) {
		MHRETURN(EDSTDIED);
	}
	dst_ptr = proc_addr_vm(dst_vmid, dst_p);
	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(EDSTDIED);
	}
	if (	(dst_ptr->p_misc_flags & VIRTUAL_TASK) ||
	        (dst_ptr->p_misc_flags & PROMICUOUS_TASK)) {
		MHRETURN(EBADSRCDST);
	}

	callerX_ptr =  proc_addr_vm(dst_vmid, priv(caller_ptr)->s_nr[dst_vmid]);
#ifdef IPC_DBG
	MHDEBUG(PROC_FORMAT, PROC_FIELDS(callerX_ptr));
#endif /* IPC_DBG */

	if ( (caller_ptr->p_misc_flags & REPLY_PENDING)) { /* SENDRECVM */
		caller_ptr->p_misc_flags |= VMID_PENDING;
	}

	/* Check if 'dst' is blocked waiting for this message. The destination's
	* SENDING flag may be set when its SENDREC call blocked while sending.
	*/
	if ( ((dst_ptr->p_rts_flags & (RECEIVING | SENDING)) == RECEIVING ) ) {

		MHASSERT( (!(dst_ptr->p_misc_flags & NOT_GETFROM)));

		if ((dst_ptr->p_getfrom_e == ANY) ||
		        (dst_ptr->p_getfrom_e == callerX_ptr->p_endpoint)) {
			/* Destination is indeed waiting for this message. */
			CopyMess_ep(callerX_ptr->p_endpoint, caller_ptr, m_ptr, dst_ptr,dst_ptr->p_messbuf);
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
#endif /* IPC_DBG */
			dst_ptr->p_getfrom_e = NONE;
			if ((dst_ptr->p_rts_flags &= ~RECEIVING) == 0) {
				enqueue(dst_ptr);
			}
			return(OK);
		}
	}

#ifdef IPC_DBG
	if ( !(caller_ptr->p_misc_flags & REPLY_PENDING)) { /* NOT SENDRECVM */
		MHDEBUG("MH_sendvm EPROCSTATUS" PROC_FORMAT, PROC_FIELDS(dst_ptr));
		return(EPROCSTATUS);
	}else{
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
	}
#endif /* IPC_DBG */

	/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	* Destination is not waiting .
	* TWO POSIBLE BEHAVIOURS
	* 1) Block and Enqueu the caller (a promiscuous process on VM0 is critical)
	* 2) copy the message to descriptor on VMx and enqueue it. The promiscuous process on VM0 remains running
	*    But any other IPC must be executed for VMx until the descriptor on VMx must be PSEUDO-READY.
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

#define	MH_BLOCK_CALLER 1

#if	MH_BLOCK_CALLER
	caller_ptr->p_messbuf = m_ptr;
	if (caller_ptr->p_rts_flags == 0) {
		dequeue(caller_ptr);
	}
	caller_ptr->p_rts_flags |= SENDING;
	caller_ptr->p_sendto_e = dst_ep;

	/* Process is now blocked.  Put in on the destination's queue. */
	xpp = &dst_ptr->p_caller_q;		/* find end of list */
	while (*xpp != NIL_PROC) {
		xpp = &(*xpp)->p_q_link;
	}
	*xpp = caller_ptr;			/* add caller to end */
	caller_ptr->p_q_link = NIL_PROC;	/* mark new end of list */
#ifdef IPC_DBG
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif /* IPC_DBG */

#else   /* MH BLOCK_CALLER */
	CopyMess_ep(callerX_ptr->p_endpoint, caller_ptr, m_ptr, caller_ptr,caller_ptr->p_messbuf);
	caller_ptr->p_rts_flags |= SENDING;
	caller_ptr->p_sendto_e = dst_ep;

	/* Process is now blocked.  Put in on the destination's queue. */
	xpp = &dst_ptr->p_caller_q;		/* find end of list */
	while (*xpp != NIL_PROC) {
		xpp = &(*xpp)->p_q_link;
	}
	*xpp = caller_ptr;				/* add caller to end */
	caller_ptr->p_q_link = NIL_PROC;		/* mark new end of list */

#endif  /* MH BLOCK_CALLER */


	return(OK);
}

/*===========================================================================*
 *				MH_recvvm				     *
 *===========================================================================*/
PRIVATE int MH_recvvm(caller_ptr, src_ep, m_ptr, src_vmid)
register struct proc *caller_ptr;	/* process trying to get message */
int src_ep;				/* which message source is wanted */
message *m_ptr;			/* pointer to message buffer */
int src_vmid;			/* source vmid */
{
	/* A process or task wants to get a message.  If a message is already queued,
	 * acquire it and deblock the sender.  If no message from the desired source
	 * is available block the caller, unless the flags don't allow blocking.
	 */
	register struct proc **xpp, *callerX_ptr;
	sys_map_t *map;
	bitchunk_t *chunk;
	int i, src_id, src_proc_nr, src_p;
	register struct proc  *src_ptr;
	static message m;
	message *my_ptr;
	int vmid, cond, rcode, not_flag;

	caller_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM);

	rcode = OK;
	not_flag = FALSE;

	if (!(caller_ptr->p_misc_flags & REPLY_PENDING)) { /* RECVVM */

		if( src_vmid != ANY_VM ) {
			if( src_ep == ANY ) {
				src_p = src_ep;
			} else {
				if( src_ep & NOT_PROC) {
					MHDEBUG("RECVVM: src_ep=%d \n",src_ep);
					not_flag = TRUE;
					src_ep &= (~NOT_PROC);
					src_p = src_ep;
				} else {
					if( !isokendpt_vm(src_vmid, src_ep, &src_p)) {
						rcode=ESRCDIED;
					}
				}
			}

		} else { /*  ANY_VM*/
			if( src_ep == ANY ) {
				src_p = src_ep;
			} else {
				if( src_ep & NOT_PROC) {
					not_flag = TRUE;
					MHDEBUG("RECVVM: src_ep=%d \n",src_ep);
					src_ep &= (~NOT_PROC);
					src_p = src_ep;
				} else {
					rcode=EBADSRCDST;
				}
			}
		}
	} else {		/* SENDRECVM */
		if( !isokendpt_vm(src_vmid, src_ep, &src_p)) {
			rcode=ESRCDIED;
		}
	}
	if(rcode) {
		/* ONLY FOR DEBUGGING */
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
		MHRETURN(rcode);
	}

	/* Check to see if a message from desired source is already available.
	* The caller's SENDING flag may be set if SENDREC couldn't send. If it is
	* set, the process should be blocked.
	*/
	if (!(caller_ptr->p_misc_flags & REPLY_PENDING)) {

		if ( (src_vmid != ANY_VM) && (src_vmid != VM0) ) { /* ONLY HARDWARE */
			src_id = priv(hard_ptr)->s_id;
			if( get_sys_bit(caller_ptr->p_priv->s_notify_pending,src_id)) { /* DID HARDWARE notify me? */

				if( --priv(caller_ptr)->s_ntfy_cnt[src_id] == 0) {
					unset_sys_bit(caller_ptr->p_priv->s_notify_pending,src_id);
				} else {
					MHDEBUG("MH_recvvm1 dst=%s src=%d src_id=%d ntfy_cnt=%d\n",
					        caller_ptr->p_name, HARDWARE, src_id,
					        priv(caller_ptr)->s_ntfy_cnt[src_id]);
				}
				/* Found a suitable source, deliver the notification message. */
				BuildMess_ptr(&m, hard_ptr, caller_ptr);	/* assemble message */
				CopyMess(HARDWARE, hard_ptr, &m, caller_ptr, m_ptr);
				return(src_vmid);				/* report VMID  */
			}

		} else 	if (((src_vmid == ANY_VM) || (src_vmid == VM0) )) {
			/* Check if there are pending notifications, except for SENDREC. 	*/
			map = &priv(caller_ptr)->s_notify_pending;
			for (chunk=&map->chunk[0]; chunk<&map->chunk[NR_SYS_CHUNKS]; chunk++) {
				/* Find a pending notification from the requested source. */
				if (! *chunk) {
					continue;    /* no bits in chunk */
				}
				for (i=0; ! (*chunk & (1<<i)); ++i) {} 	/* look up the bit */
				src_id = (chunk - &map->chunk[0]) * BITCHUNK_BITS + i;
				if (src_id >= NR_SYS_PROCS) {
					break;    /* out of range */
				}
				src_proc_nr = id_to_nr(src_id);		/* get source proc */
				if( not_flag == TRUE) {
					if( src_p == src_proc_nr) {
#ifdef IPC_DBG
						/* ONLY FOR DEBUGGING */
						MHDEBUG("MH_recvvm: src_p=%d src_proc_nr=%d\n", src_p,src_proc_nr);
#endif /* IPC_DBG */
						continue; /* ignore this notify */
					}
				} else {
					if (src_ep!=ANY && src_p != src_proc_nr) {
						continue;    /* source not ok */
					}
				}

				if( --priv(caller_ptr)->s_ntfy_cnt[src_id] == 0) {
					*chunk &= ~(1 << i);    /* no longer pending */
				} else
					MHDEBUG("MH_recvvm2 dst=%s src=%d src_id=%d ntfy_cnt=%d\n",
					        caller_ptr->p_name, src_proc_nr, src_id,
					        priv(caller_ptr)->s_ntfy_cnt[src_id]);

				/* Found a suitable source, deliver the notification message. */
				BuildMess(&m, src_proc_nr, caller_ptr);	/* assemble message */
				CopyMess(src_proc_nr, hard_ptr, &m, caller_ptr, m_ptr);
				return(VM0);				/* report VMID  */
			}
		}
	}

	/* ONLY FOR DEBUGGING */
	if( not_flag == TRUE) {
		MHDEBUG("RECVVM: SEARCHING NOT src_ep=%d \n",src_ep);
	}

	xpp = &caller_ptr->p_caller_q;
	while (*xpp != NIL_PROC) {
		/* discard unwanted process number .  It wants ANY except src_p */
		if(not_flag == TRUE) {
			if( (src_p == (*xpp)->p_nr) &&
			        ((src_vmid == ANY_VM || src_vmid == (*xpp)->p_vmid))) {

				/* ONLY FOR DEBUGGING */
				MHDEBUG("RECVVM: MATCH NOT src_p=%d \n",src_p);
				cond = FALSE;
			} else {
				cond = TRUE;
			}
		} else if ( ((src_ep == ANY) || (src_ep == (*xpp)->p_endpoint))  &&
		            ((src_vmid == (*xpp)->p_vmid) || (src_vmid == ANY_VM)) ) {
			cond = TRUE;
		} else {
			cond = FALSE;
		}

		if(cond == TRUE) {
			vmid = (*xpp)->p_vmid;

			if( vmid != VM0) {
#ifdef IPC_DBG
				/* ONLY FOR DEBUGGING */
				my_ptr = &m;
				CopyMess_ep((*xpp)->p_endpoint, *xpp, (*xpp)->p_messbuf, proc_addr(HARDWARE), my_ptr);
				MHDEBUG("MH_recvvm: src=%s dst=%s misc=%X vmid=%d " MSG1_FORMAT,
				        (*xpp)->p_name, caller_ptr->p_name, caller_ptr->p_misc_flags, vmid
				        ,MSG1_FIELDS(my_ptr));
#endif /* IPC_DBG */
				MHASSERT( (priv(caller_ptr)->s_nr[vmid] != NONE));
				callerX_ptr = proc_addr_vm(vmid, priv(caller_ptr)->s_nr[vmid]); 	/* get the caller descriptor on VMx */
				MHASSERT( (callerX_ptr->p_misc_flags & PROMICUOUS_TASK));
				MHASSERT( (priv(caller_ptr)->s_vm_bitmap & (1 << vmid)));

				if( (*xpp)->p_rts_flags & NOTIFYING) {	/* is it a NOTIFY message from VMx */
					/* Found a suitable source, deliver the notification message. */
					BuildMess_ptr(&m, (*xpp), caller_ptr); /* assemble message */
					CopyMess_ep((*xpp)->p_endpoint, hard_ptr, &m, caller_ptr, m_ptr);
					if (((*xpp)->p_rts_flags &= ~NOTIFYING) == 0) {
						enqueue(*xpp);
					}
				} else {
					CopyMess_ep((*xpp)->p_endpoint, *xpp, (*xpp)->p_messbuf,caller_ptr, m_ptr);
					if (((*xpp)->p_rts_flags &= ~SENDING) == 0) {
						enqueue(*xpp);
					}
				}
			} else {
				CopyMess_ep((*xpp)->p_endpoint, *xpp, (*xpp)->p_messbuf,caller_ptr, m_ptr);
				if (((*xpp)->p_rts_flags &= ~SENDING) == 0) {
					enqueue(*xpp);
				}
			}

#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			if( vmid != VM0) {
				MHDEBUG(PROC_FORMAT, PROC_FIELDS((*xpp)));
				my_ptr = &m;
				CopyMess_ep((*xpp)->p_endpoint, (*xpp), (*xpp)->p_messbuf, proc_addr(HARDWARE), my_ptr);
				MHDEBUG("MH_recvvm: src=%s dst=%d vmid=%d " MSG1_FORMAT,
				        caller_ptr->p_name, src_ep, vmid, MSG1_FIELDS(my_ptr));
			}
#endif /* IPC_DBG */

			/* (*xpp)->p_send_vm  = ANY_VM; */
			(*xpp)->p_sendto_e = NONE;
			*xpp = (*xpp)->p_q_link;		/* remove from queue */
			caller_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM | REPLY_PENDING);
			caller_ptr->p_recv_vm = ANY_VM;

			return(vmid);
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
	}

	/* No suitable message is available or the caller couldn't send in SENDREC.
	* Block the process trying to receive, unless the flags tell otherwise.
	*/

	caller_ptr->p_getfrom_e = src_ep;
	caller_ptr->p_messbuf = m_ptr;
	if (caller_ptr->p_rts_flags == 0) {
		dequeue(caller_ptr);
	}
	caller_ptr->p_rts_flags |= RECEIVING;
	caller_ptr->p_misc_flags |= VMID_PENDING;
	if( not_flag == TRUE) {
		caller_ptr->p_misc_flags |= NOT_GETFROM;
		MHDEBUG("RECVVM: src_ep=%d p_misc_flags=%X\n",src_ep, caller_ptr->p_misc_flags);
	}
	caller_ptr->p_recv_vm = src_vmid;

	return(OK);
}


/*===========================================================================*
 *				MH_ntfyvm				     *
 *===========================================================================*/
PRIVATE int MH_ntfyvm(caller_ptr, dst_p, dst_vmid)
register struct proc *caller_ptr;	/* sender of the notification */
int dst_p;				/* which process to notify */
int dst_vmid;
{
	register struct proc **xpp;
	register struct proc *dst_ptr, *callerX_ptr;
	int src_id;
	message m;

#ifdef IPC_DBG
	/* ONLY FOR DEBUGGING */
	if(dst_vmid > VM0)
		MHDEBUG("MH_ntfyvm src=%s dst=%d flags=%d dst_vmid=%d\n",
		        caller_ptr->p_name, dst_p, caller_ptr->p_rts_flags, dst_vmid);
#endif /* IPC_DBG */

	/* Verify correct Destination  */
	dst_ptr = proc_addr_vm(dst_vmid, dst_p);
	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(EDSTDIED);
	}

	MHASSERT(!(dst_ptr->p_misc_flags & VIRTUAL_TASK));
	MHASSERT(!(dst_ptr->p_misc_flags & PROMICUOUS_TASK));

	callerX_ptr = proc_addr_vm(dst_vmid, priv(caller_ptr)->s_nr[dst_vmid]); 	/* get the caller descriptor on VMx */
#ifdef IPC_DBG
	MHDEBUG(PROC_FORMAT, PROC_FIELDS(callerX_ptr));
#endif /* IPC_DBG */

	/* Check to see if target is blocked waiting for this message. A process
	* can be both sending and receiving during a SENDREC system call.
	*/
	if ((dst_ptr->p_rts_flags & (RECEIVING|SENDING)) == RECEIVING &&
	        ! (dst_ptr->p_misc_flags & REPLY_PENDING)) {

		MHASSERT( (!(dst_ptr->p_misc_flags & NOT_GETFROM)));

		if ( (dst_ptr->p_getfrom_e == ANY) ||
		        (dst_ptr->p_getfrom_e == callerX_ptr->p_endpoint)) {
			/* Destination is indeed waiting for a message. Assemble a notification
			* message and deliver it. Copy from pseudo-source HARDWARE, since the
			* message is in the kernel's address space.
			*/
			BuildMess_ptr(&m, callerX_ptr, dst_ptr);
			CopyMess_ep(callerX_ptr->p_endpoint, hard_ptr, &m,
			            dst_ptr, dst_ptr->p_messbuf);
			dst_ptr->p_getfrom_e = NONE;
			dst_ptr->p_rts_flags &= ~RECEIVING;	/* deblock destination */
			if (dst_ptr->p_rts_flags == 0) {
				enqueue(dst_ptr);
			}
			return(OK);
		}
	}

	/* Destination is not ready to receive the notification. Add it to the
	 * bit map with pending notifications. Note the indirectness: the system id
	* instead of the process number is used in the pending bit map.
	*/
	src_id = priv(caller_ptr)->s_id;
	set_sys_bit(priv(dst_ptr)->s_notify_pending, src_id);
	priv(dst_ptr)->s_ntfy_cnt[src_id]++;
	/*	if( priv(dst_ptr)->s_ntfy_cnt[src_id] != 1) */
	MHDEBUG("MH_ntfyvm dst=%s src=%s src_id=%d ntfy_cnt=%d\n",
	        dst_ptr->p_name, caller_ptr->p_name, src_id,
	        priv(dst_ptr)->s_ntfy_cnt[src_id] );

	return(OK);
}


/*===========================================================================*
 *				MH_relay				     						*
 *===========================================================================*/
PRIVATE int MH_relay(caller_ptr, dst_ep, m_ptr, src_ep)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dst_ep;				/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
int src_ep;				/* from whom is message being sent? */
{
	/* Send a message from 'caller_ptr' to 'dst_ep' as it was sent by src_ep.
	* If 'dst_ep' is blocked waiting  for this message, copy the message to it and unblock 'dst_ep'.
	* src_ep must has made a sendrec() to caller and it must be waiting for the response
	*/
	register struct proc *dst_ptr , *src_ptr, *dst0_ptr, *dstX_ptr;
	register struct proc **xpp;
	int dst_p, src_p, proxy_ep,proxy_nr;

	if( !isokendpt(dst_ep, &dst_p)) {
		MHRETURN(EDSTDIED);
	}
	dst_ptr = proc_addr(dst_p);
	if (dst_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(EDSTDIED);
	}

	if( !isokendpt(src_ep, &src_p)) {
		MHRETURN(ESRCDIED);
	}
	src_ptr = proc_addr(src_p);
	if (src_ptr->p_rts_flags & NO_ENDPOINT) {
		MHRETURN(ESRCDIED);
	}
	/*  The emulated source must be blocked RECEIVING and with its REPLY_PENDING bit set */
	if( !(src_ptr->p_misc_flags & REPLY_PENDING)
	        || !((src_ptr->p_rts_flags & (RECEIVING | SENDING)) == RECEIVING) ) {
		MHRETURN(EPROCSTATUS);
	}
	if( caller_ptr->p_vmid != src_ptr->p_vmid) {
		MHRETURN(EBADVMID);
	}


	if( caller_ptr->p_vmid != VM0)  {

		/* caller within VMx and destination Promiscuous ? */
		if (dst_ptr->p_misc_flags & PROMICUOUS_TASK) {
			dst0_ptr = proc_addr_vm(VM0, dst_p);
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst0_ptr));
#endif /* IPC_DBG */

			/* is the promiscuous process configured to serve that VM? */
			if( (priv(dst0_ptr)->s_vm_bitmap & (1 << caller_ptr->p_vmid))) {
				dstX_ptr = dst_ptr;
				dst_ptr  = dst0_ptr;
			} else {
				MHRETURN(EPROMISCUOUS);
			}

			/* caller within VMx and destination Virtual? */
		} else if (dst_ptr->p_misc_flags &  VIRTUAL_TASK) {
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
#endif /* IPC_DBG */
			proxy_ep =  vm_hyper[running_vm].vm_VMM.vmm_endpoint;
			if( !isokendpt_vm(VM0, proxy_ep, &proxy_nr)) {
				MHRETURN(EVIRTUALTASK);
			}
			dst0_ptr = proc_addr_vm(VM0, proxy_nr);

			/* is the destination the proxy of the VM? */
			if( dst0_ptr->p_proxy != caller_ptr->p_vmid) {
				MHRETURN(EVIRTUALTASK);
			}
			dstX_ptr = dst_ptr;

			/* caller within VMx and destination real within VMx */
		} else {
			dstX_ptr = dst_ptr;
		}

	} else { /* caller within VM0 and destination real within VM0 */
		dstX_ptr = dst_ptr;
	}
	/* OUTPUT
	*  dst_ptr = destination mentioned in the IPC
	*  dstX_ptr = destination mentioned in the IPC within a VM
	*  dst0_ptr = promiscuous/proxy process in VM0
	*/

	/* Check if 'dst' is blocked waiting for this message. The destination's
	* SENDING flag may be set when its SENDREC call blocked while sending.
	*/
	do {
		if ( (dst_ptr->p_rts_flags & (RECEIVING | SENDING)) == RECEIVING ) {

			if( dst_ptr->p_misc_flags & VMID_PENDING ) { /* DESTINATION IS PROMISCUOUS */

				/* Only Promiscuous processes can use receivevm() and NOT_PROC flag, waiting with NOT_GETFROM  misc flag set */
				if( (dst_ptr->p_misc_flags & NOT_GETFROM) &&
				        (dst_ptr->p_getfrom_e == caller_ptr->p_nr)) {
					MHDEBUG("MH_send MATCH caller_nr=%d dst=%d\n", caller_ptr->p_nr, dst_ptr->p_endpoint);
					break;
				}

				if(	((dst_ptr->p_recv_vm == ANY_VM)  ||
				        (dst_ptr->p_recv_vm == caller_ptr->p_vmid))
				        &&
				        ((dst_ptr->p_getfrom_e == caller_ptr->p_endpoint)  ||
				         (dst_ptr->p_getfrom_e == ANY) ||
				         (dst_ptr->p_misc_flags & NOT_GETFROM))) {
					/* Destination is indeed waiting for this message. */

#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if( dst_ptr->p_misc_flags & NOT_GETFROM) {
						MHDEBUG("MH_send caller_nr=%d dst=%d\n", caller_ptr->p_nr, dst_ptr->p_endpoint);
					}
#endif /* IPC_DBG */

					CopyMess_ep(src_ptr->p_endpoint, caller_ptr, m_ptr, dst_ptr,dst_ptr->p_messbuf);
					/* the source, now must wait the response from destination */
					src_ptr->p_getfrom_e = dst_ptr->p_endpoint;

					/* Return the caller's VMID */
					dst_ptr->p_reg.retreg = caller_ptr->p_vmid;
					dst_ptr->p_misc_flags &= ~(VMID_PENDING | NOT_GETFROM | REPLY_PENDING);
					dst_ptr->p_getfrom_e = NONE;
					dst_ptr->p_recv_vm = ANY_VM;
					if ((dst_ptr->p_rts_flags &= ~RECEIVING) == 0) {
						enqueue(dst_ptr);
					}
#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if(caller_ptr->p_vmid > VM0) {
						MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
					}
#endif /* IPC_DBG */
					return(OK);
				}

			} else { /*  DESTINATION IS NOT PROMISCUOUS */
				MHASSERT( (!(dst_ptr->p_misc_flags & NOT_GETFROM)));

				if ( ((dst_ptr->p_vmid == caller_ptr->p_vmid) ||
				        (dst_ptr->p_nr == SYSTEM))
				        &&
				        ((dst_ptr->p_getfrom_e == ANY) ||
				         (dst_ptr->p_getfrom_e == caller_ptr->p_endpoint))) {

					/* SYSTASK inherits the caller's VMID */
					if( dst_ptr->p_nr == SYSTEM) {
						systask_ptr->p_vmid = caller_ptr->p_vmid;
					}

					/* Destination is indeed waiting for this message. */
					CopyMess_ep(src_ptr->p_endpoint, caller_ptr, m_ptr, dst_ptr,dst_ptr->p_messbuf);
					/* the source, now must wait the response from destination */
					src_ptr->p_getfrom_e = dst_ptr->p_endpoint;

					dst_ptr->p_misc_flags &= ~(REPLY_PENDING);
					dst_ptr->p_getfrom_e = NONE;
					dst_ptr->p_recv_vm = ANY_VM;
					if ((dst_ptr->p_rts_flags &= ~RECEIVING) == 0) {
						enqueue(dst_ptr);
					}
#ifdef IPC_DBG
					/* ONLY FOR DEBUGGING */
					if(caller_ptr->p_vmid > VM0) {
						MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
					}
#endif /* IPC_DBG */
					return(OK);
				}
			}
		}
	} while(0);

	/* SYSTASK CAN'T WAIT !! */
	MHASSERT( (caller_ptr->p_nr != SYSTEM));
	/* Destination is not waiting the message.  source must waitfor destination */

	/* copy the modifyed message  to the source process descriptor  */
	CopyMess_ep(src_ptr->p_endpoint, caller_ptr, m_ptr, src_ptr, src_ptr->p_messbuf);

	/* add the SENDING flag as if the source never sent the message */
	src_ptr->p_rts_flags |= SENDING;
	src_ptr->p_sendto_e = dst_ep;

	/* Process is now blocked.  Put in on the destination's queue. */
	xpp = &dst_ptr->p_caller_q;		/* find end of list */
	while (*xpp != NIL_PROC) {
		xpp = &(*xpp)->p_q_link;
	}
	*xpp = src_ptr;					/* add src to end */
	src_ptr->p_q_link = NIL_PROC;	/* mark new end of list */

	if( caller_ptr->p_vmid != VM0)  {
		/* if destination is a VIRTUAL TASK with its proxy not receiving, notifies the proxy */
		if (dst_ptr->p_misc_flags &  VIRTUAL_TASK) {
#ifdef IPC_DBG
			/* ONLY FOR DEBUGGING */
			MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
#endif /* IPC_DBG */
			return(notify_proxy(priv(dst_ptr)->s_id));
		}
	}

	return(OK);
}


/*===========================================================================*
 *				enqueue					     *
 *===========================================================================*/
PRIVATE void enqueue(rp)
register struct proc *rp;	/* this process is now runnable */
{
	/* Add 'rp' to one of the queues of runnable processes.  This function is
	 * responsible for inserting a process into one of the scheduling queues.
	 * The mechanism is implemented here.   The actual scheduling policy is
	 * defined in sched() and pick_proc().
	 */
	int q;	 				/* scheduling queue to use */
	int front;					/* add to front or back */

	/* Determine where to insert to process. */
	sched(rp, &q, &front);

	if(rp->p_nr == CLOCK && proc_ptr->p_nr == SYSTEM) {
		front=0;	/* SYSTEM has priority over CLOCK */
	} else if(rp->p_nr == SYSTEM && proc_ptr->p_nr == CLOCK) {
		front=0;	/* CLOCK has priority over SYSTEM */
	}

	/* Now add the process to the queue. */
	if (rdy_head[q] == NIL_PROC) {		/* add to empty queue */
		rdy_head[q] = rdy_tail[q] = rp; 		/* create a new queue */
		rp->p_nextready = NIL_PROC;		/* mark new end */
	} else if (front) {				/* add to head of queue */
		rp->p_nextready = rdy_head[q];		/* chain head of queue */
		rdy_head[q] = rp;				/* set new queue head */
	} else {					/* add to tail of queue */
		rdy_tail[q]->p_nextready = rp;		/* chain tail of queue */
		rdy_tail[q] = rp;				/* set new queue tail */
		rp->p_nextready = NIL_PROC;		/* mark new end */
	}

	/* Now select the next process to run. */
	pick_proc();

}

/*===========================================================================*
 *				dequeue					     *
 *===========================================================================*/
PRIVATE void dequeue(rp)
register struct proc *rp;	/* this process is no longer runnable */
{
	/* A process must be removed from the scheduling queues, for example, because
	 * it has blocked.  If the currently active process is removed, a new process
	 * is picked to run by calling pick_proc().
	 */
	register int q = rp->p_priority;		/* queue to use */
	register struct proc **xpp;			/* iterate over queue */
	register struct proc *prev_xp;

	/* Side-effect for kernel: check if the task's stack still is ok? */
	if (iskernelp(rp)) {
		if (*priv(rp)->s_stack_guard != STACK_GUARD) {
			panic("stack overrun by task", proc_nr(rp));
		}
	}

	/* Now make sure that the process is not in its ready queue. Remove the
	 * process if it is found. A process can be made unready even if it is not
	 * running by being sent a signal that kills it.
	 */
	prev_xp = NIL_PROC;
	for (xpp = &rdy_head[q]; *xpp != NIL_PROC; xpp = &(*xpp)->p_nextready) {

		if (*xpp == rp) {				/* found process to remove */
			*xpp = (*xpp)->p_nextready;		/* replace with next chain */
			if (rp == rdy_tail[q]) {	/* queue tail removed */
				rdy_tail[q] = prev_xp;    /* set new tail */
			}
			if (rp == proc_ptr || rp == next_ptr) {	/* active process removed */
				pick_proc();    /* pick new process to run */
			}
			break;
		}
		prev_xp = *xpp;				/* save previous in chain */
	}

}

/*===========================================================================*
 *				sched					     *
 *===========================================================================*/
PRIVATE void sched(rp, queue, front)
register struct proc *rp;			/* process to be scheduled */
int *queue;					/* return: queue to use */
int *front;					/* return: front or back */
{
	/* This function determines the scheduling policy.  It is called whenever a
	 * process must be added to one of the scheduling queues to decide where to
	 * insert it.  As a side-effect the process' priority may be updated.
	 */
	int time_left = (rp->p_ticks_left > 0);	/* quantum fully consumed */

	/* Check whether the process has time left. Otherwise give a new quantum
	 * and lower the process' priority, unless the process already is in the
	 * lowest queue.
	 */
	if (! time_left) {				/* quantum consumed ? */
		rp->p_ticks_left = rp->p_quantum_size; 	/* give new quantum */
		if (rp->p_priority < (IDLE_Q-1)) {
			rp->p_priority += 1;			/* lower priority */
		}
	}

	/* If there is time left, the process is added to the front of its queue,
	 * so that it can immediately run. The queue to use simply is always the
	 * process' current priority.
	 */
	*queue = rp->p_priority;
	*front = time_left;
}


/*===========================================================================*
 *				pick_proc				     *
 *===========================================================================*/
PRIVATE void pick_proc()
{
	/* Decide who to run now.  A new process is selected by setting 'next_ptr'.
	 * When a billable process is selected, record it in 'bill_ptr', so that the
	 * clock task can tell who to bill for system time.
	 */
	register struct proc *rp;			/* process to run */
#ifdef MHYPER
	struct proc *rp_TwT;				/* highest priority Task within a VM with Tokens  */
	struct proc *rp_PwT;				/* highest priority Process  within a VM with Tokens  */
	struct proc *rp_TwoT;				/* highest priority Task  within a VM with out Tokens  */
	struct proc *rp_PwoT;				/* highest priority Process  within a VM with  out Tokens  */
	struct proc *rp_nvmT;				/* highest priority Process  within next_VM  (with  out Tokens)  */

	register struct vm_struct *rvm;		/* virtual machine to run */
#endif /*MHYPER*/
	int q;			/* iterate over queues */

	/* Check each of the scheduling queues for ready processes. The number of
	 * queues is defined in proc.h, and priorities are set in the task table.
	 * The lowest queue contains IDLE, which is always ready.
	 */

#ifdef MHYPER

	rp_TwT	= NULL;
	rp_PwT	= NULL;
	rp_TwoT = NULL;
	rp_PwoT = NULL;
	rp_nvmT = NULL;

	for (q = 0 ; q < (NR_SCHED_QUEUES-1) ; q++) {	/* searches all queues */
		rp = rdy_head[q];
		while( rp != NIL_PROC) {					/* searches all  processes in queue */
			next_ptr = rp;

			/* FIRST: SYSTEM and CLOCK */
			if(rp->p_nr == SYSTEM || rp->p_nr == CLOCK) {
				return;
			}

			/* SECOND:  Highest priority process from VM0 */
			if(rp->p_vmid == VM0) { /* Select VM0 first of all */
				if (priv(rp)->s_flags & BILLABLE) {
					bill_ptr = rp;    /* bill for system time */
				}
				return;
			}

			rvm = &vm_hyper[rp->p_vmid];

			/* THIRD:  Task within VM with Tokens */
			if( rp_TwT	== NULL) {
				if( (MH_LEVEL(rp->p_misc_flags) == TASK_LEVEL) &&
				        (rvm->vm_tokens > 0)) {
					rp_TwT = rp;
				}
			} else

				/* FORTH:  Process of within next_vm with Tokens */
				if( rp_nvmT == NULL && next_vm != VM0) {
					if( rp->p_vmid == next_vm) {
						rp_nvmT = rp;
					}
				} else

					/* FIFTH:  Process  within VM with Tokens */
					if( rp_PwT == NULL) {
						if( (MH_LEVEL(rp->p_misc_flags) != TASK_LEVEL) &&
						        (rvm->vm_tokens > 0)) {
							rp_PwT = rp;
						}
					} else

						/* SIXTH:  Task  within VM without Tokens */
						if( rp_TwoT == NULL) {
							if( MH_LEVEL(rp->p_misc_flags) == TASK_LEVEL ) {
								rp_TwoT = rp;
							}
						}

			/* SEVEN:  Processes  within VM without Tokens */
			if( rp_PwoT == NULL) {
				rp_PwoT = rp;
			}
			rp = rp->p_nextready;
		}
	}

	if( rp_TwT	!= NULL) {  /* THIRD:  Task within VM with Tokens */
		next_ptr = rp_TwT;
	} else if( rp_nvmT != NULL) {/* FORTH:  Process of within next_vm with Tokens */
		next_ptr = rp_nvmT;
	} else if( rp_PwT != NULL) { /* FIFTH:  Process  within VM with Tokens */
		next_ptr = rp_PwT;
	} else if( rp_TwoT != NULL) { /* SIXTH:  Task  within VM with Tokens */
		next_ptr = rp_TwoT;
	} else if( rp_PwoT != NULL) { /* SEVENTH: Process within VM without Tokens */
		next_ptr = rp_PwoT;
	} else {
		next_ptr =  rdy_head[IDLE_Q]; /* LAST: IDLE */
	}

	if (priv(next_ptr)->s_flags & BILLABLE) {
		bill_ptr = next_ptr;    /* bill for system time */
	}
	return;

#else
	for (q=0; q < NR_SCHED_QUEUES; q++) {
		if ( (rp = rdy_head[q]) != NIL_PROC) {
			next_ptr = rp;			/* run process 'rp' next */
			if (priv(rp)->s_flags & BILLABLE) {
				bill_ptr = rp;    /* bill for system time */
			}
			return;
		}
	}
#endif /* MHYPER */
}

/*===========================================================================*
 *				balance_queues				     *
 *===========================================================================*/
#define Q_BALANCE_TICKS	 100
PUBLIC void balance_queues(tp)
timer_t *tp;					/* watchdog timer pointer */
{
	/* Check entire process table and give all process a higher priority. This
	 * effectively means giving a new quantum. If a process already is at its
	 * maximum priority, its quantum will be renewed.
	 */
	static timer_t queue_timer;			/* timer structure to use */
	register struct proc* rp;			/* process table pointer  */
	clock_t next_period;				/* time of next period  */
	int ticks_added = 0;				/* total time added */
	int vmid;

#ifdef MHYPER
	return;
#endif /* MHYPER */

	lock(5,"balance_queues");
	for(vmid=0; vmid < NR_VMS; vmid++) {
		if(vm_hyper[vmid].vm_flags !=  (VM_LOADED | VM_RUNNING)) {
			continue;
		}
		for (rp=&vm_hyper[vmid].vm_proc[NR_TASKS];
		        rp<&vm_hyper[vmid].vm_proc[NR_TASKS + NR_PROCS];
		        rp++) {
			if (!isemptyp(rp)) {				/* check slot use */
				if (rp->p_priority > rp->p_max_priority) {	/* update priority? */
					if (rp->p_rts_flags == 0) {
						dequeue(rp);    /* take off queue */
					}
					ticks_added += rp->p_quantum_size;	/* do accounting */
					rp->p_priority -= 1;			/* raise priority */
					if (rp->p_rts_flags == 0) {
						enqueue(rp);    /* put on queue */
					}
				} else {
					ticks_added += rp->p_quantum_size - rp->p_ticks_left;
					rp->p_ticks_left = rp->p_quantum_size; 	/* give new quantum */
				}
			}
		}
	}
	unlock(5);

	/* Now schedule a new watchdog timer to balance the queues again.  The
	   * period depends on the total amount of quantum ticks added.
	   */
	next_period = MAX(Q_BALANCE_TICKS, ticks_added);	/* calculate next */
	set_timer(&queue_timer, get_uptime() + next_period, balance_queues);
}

/*===========================================================================*
 *				lock_send				     *
 *===========================================================================*/
PUBLIC int lock_send(dst_ep, m_ptr)
int dst_ep;			/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
{
	/* Safe gateway to MH_send() for tasks. */
	int result;

	/*#define TTY_BUG 1 */
#ifdef TTY_BUGXXX
	/******************* ONLY FOR DEBUGGING ****************************/
	if (proc_ptr->p_nr == SYSTEM && dst_ep ==  TTY_PROC_NR ) {
		MHDEBUG("LOCKSEND src=%s dst=%d \n",proc_ptr->p_name, dst_ep);
	}
#endif /* TTY_BUG */

	lock(2, "send");
	result = MH_send(proc_ptr, dst_ep, m_ptr);
	if(result) {
		MHDEBUG("LOCKSEND src=%s dst=%d \n",proc_ptr->p_name, dst_ep);
	}
	unlock(2);
	return(result);

}

/*===========================================================================*
 *				lock_notify				     *
 *===========================================================================*/
PUBLIC int lock_notify(src_e, dst_e)
int src_e;			/* (endpoint) sender of the notification */
int dst_e;			/* (endpoint) who is to be notified */
{
	/* Safe gateway to mini_notify() for tasks and interrupt handlers. The sender
	 * is explicitely given to prevent confusion where the call comes from. MINIX
	 * kernel is not reentrant, which means to interrupts are disabled after
	 * the first kernel entry (hardware interrupt, trap, or exception). Locking
	 * is done by temporarily disabling interrupts.
	 */
	int result_src, result_dst, result,src, dst;

	result_src = !isokendpt(src_e, &src);
	if(result_src) {
		panic("lock_notify1: src",src);
	}

	result_dst = !isokendpt(dst_e, &dst);
	if(result_dst) {
		panic("lock_notify2: dst",dst);
	}

	if(result_src || result_dst) {
		return EDEADSRCDST;
	}

	/* Exception or interrupt occurred, thus already locked. */
	if (k_reenter >= 0) {
		result = raw_notify(src, dst);
	}  /* Call from task level, locking is required. */
	else {
		lock(0, "notify");
		result = raw_notify(src, dst);
		unlock(0);
	}
	if(result ) {
		panic("lock_notify3: src",src);
	}

	return(result);
}

/*===========================================================================*
 *				lock_enqueue				     *
 *===========================================================================*/
PUBLIC void lock_enqueue(rp)
struct proc *rp;		/* this process is now runnable */
{
	/* Safe gateway to enqueue() for tasks. */
	lock(3, "enqueue");
	enqueue(rp);
	unlock(3);
}

/*===========================================================================*
 *				lock_dequeue				     *
 *===========================================================================*/
PUBLIC void lock_dequeue(rp)
struct proc *rp;		/* this process is no longer runnable */
{
	/* Safe gateway to dequeue() for tasks. */
	if (k_reenter >= 0) {
		/* We're in an exception or interrupt, so don't lock (and ...
		 * don't unlock).
		 */
		dequeue(rp);
	} else {
		lock(4, "dequeue");
		dequeue(rp);
		unlock(4);
	}
}

/*===========================================================================*
 *				isokendpt_f				     *
 *===========================================================================*/
#if DEBUG_ENABLE_IPC_WARNINGS
PUBLIC int isokendpt_f(file, line, e, p, fatalflag)
char *file;
int line;
#else
PUBLIC int isokendpt_f(e, p, fatalflag)
#endif
int e, *p, fatalflag;
{
	int ok = 0;
	struct proc *rp;

	/* Convert an endpoint number into a process number.
	 * Return nonzero if the process is alive with the corresponding
	 * generation number, zero otherwise.
	 *
	 * This function is called with file and line number by the
	 * isokendpt_d macro if DEBUG_ENABLE_IPC_WARNINGS is defined,
	 * otherwise without. This allows us to print the where the
	 * conversion was attempted, making the errors verbose without
	 * adding code for that at every call.
	 *
	 * If fatalflag is nonzero, we must panic if the conversion doesn't
	 * succeed.
	 */

	*p = _ENDPOINT_P(e);

	if( *p == SYSTEM || *p == CLOCK || *p == HARDWARE || *p == IDLE) {
		ok = 1;
		return(ok);
	}
	rp = proc_addr(*p);

	if(!isokprocn(*p)) {
		MHDEBUG("isokendpt1: proc_ptr(%s,%d,%d) src_dst(%d,%d) running_vm=%d\n",
		        proc_ptr->p_name,proc_ptr->p_nr, proc_ptr->p_vmid, e, *p, running_vm);
	} else if(rp->p_endpoint != e) {
		if(rp->p_misc_flags & PROMICUOUS_TASK) {
			MHDEBUG("isokendpt2a: PROMICUOUS_TASK rp(%s,%d,%d) endpoint(%d!=%d) running_vm=%d\n",
			        rp->p_name,rp->p_nr,rp->p_vmid,
			        rp->p_endpoint, e,
			        running_vm);
			rp = proc_addr_vm(VM0,*p);
		}
		if(rp->p_endpoint != e) {
			MHDEBUG("isokendpt2b: proc_ptr(%s,%d,%d) rp(%s,%d,%d) endpoint(%d!=%d) running_vm=%d\n",
			        proc_ptr->p_name,proc_ptr->p_nr, proc_ptr->p_vmid,
			        rp->p_name,rp->p_nr,rp->p_vmid,
			        rp->p_endpoint, e, running_vm);
		}
	} else if(rp->p_rts_flags == SLOT_FREE) {
		MHDEBUG("isokendpt3: e=%d *p=%d proc_ptr(%s,%d,%d,%d,%X) src_dst(%s,%d,%d,%d,%X). running_vm=%d\n", e, *p,
		        proc_ptr->p_name, proc_ptr->p_endpoint, proc_ptr->p_nr, proc_ptr->p_vmid,proc_ptr->p_rts_flags,
		        rp->p_name,rp->p_endpoint, rp->p_nr, rp->p_vmid, rp->p_rts_flags, running_vm);
	} else {
		ok = 1;
	}

	if(!ok && fatalflag) {
		panic("invalid endpoint ", e);
	}
	return ok;
}

#ifdef MHYPER
/*===========================================================================*
 *				isokendpt_vm				     *
 *===========================================================================*/
PUBLIC int isokendpt_vm(vmid, e, p)
int vmid;
int e;
int *p;
{
	int true = 0;
	struct proc *rp;

	/* Convert an endpoint number into a process number.
	 * Return nonzero if the process is alive with the corresponding
	 * generation number, zero otherwise.
	 *
	 * This function is called with file and line number by the
	 * isokendpt_d macro if DEBUG_ENABLE_IPC_WARNINGS is defined,
	 * otherwise without. This allows us to print the where the
	 * conversion was attempted, making the errors verbose without
	 * adding code for that at every call.
	 *
	 * If fatalflag is nonzero, we must panic if the conversion doesn't
	 * succeed.
	 */

	*p = _ENDPOINT_P(e);

	if( *p == SYSTEM || *p == CLOCK || *p == HARDWARE || *p == IDLE) {
		true = 1;
		return(true);
	}
	rp = proc_addr_vm(vmid, *p);

	if(!isokprocn(*p)) {
		MHDEBUG("isokendpt_vm1: proc_ptr(%s,%d,%d) src_dst(%d,%d) running_vm=%d\n",
		        proc_ptr->p_name,proc_ptr->p_nr, proc_ptr->p_vmid, e, *p, running_vm);
	} else if(rp->p_endpoint != e) {
		if(rp->p_misc_flags & PROMICUOUS_TASK) {
			rp = proc_addr_vm(VM0,*p);
		}
		if(rp->p_endpoint != e) {
			MHDEBUG("isokendpt_vm2: proc_ptr(%s,%d,%d) rp(%s,%d,%d) endpoint(%d!=%d) running_vm=%d\n",
			        proc_ptr->p_name,proc_ptr->p_nr, proc_ptr->p_vmid,
			        rp->p_name,rp->p_nr,rp->p_vmid,
			        rp->p_endpoint, e, running_vm);
		}
	} else if(rp->p_rts_flags == SLOT_FREE) {
		MHDEBUG("isokendpt_vm3: proc_ptr(%s,%d,%d,%X) src_dst(%s,%d,%d,%X). running_vm=%d\n",
		        proc_ptr->p_name, proc_ptr->p_nr, proc_ptr->p_vmid,proc_ptr->p_rts_flags,
		        rp->p_name,rp->p_nr,rp->p_vmid, rp->p_rts_flags, running_vm);
	}  else {
		true = 1;
	}

	return true;
}

/*===========================================================================*
 *				mini_hdebug				     *
 * The kernel prints a user-space text (utext < 28 chars) and an a code (int)*
 *===========================================================================*/
PRIVATE int mini_hdebug(utext, code)
char *utext;
int code;
{
	char *ptr;
	static message mprint;

	cp_mess(proc_ptr->p_endpoint,
	        proc_ptr->p_memmap[D].mem_phys,
	        (vir_bytes)utext,
	        hard_ptr->p_memmap[D].mem_phys,
	        (vir_bytes) &mprint);

	ptr = (char*) &mprint;
	ptr += (sizeof(message)-2);
	*ptr = '\0';
	ptr = (char*)&mprint;
	ptr += sizeof(int);

	MHDEBUG("hdebug: name=%s vmid=%d nr=%d ep=%d %s:%d\n",
	        proc_ptr->p_name,
	        proc_ptr->p_vmid,
	        proc_ptr->p_nr,
	        proc_ptr->p_endpoint,
	        ptr, code);

	return(proc_ptr->p_vmid);
}

/*===========================================================================*
 *				mini_prtmsg				     *
 * The kernel prints a user-space message with type 1 format		     *
 *===========================================================================*/
PRIVATE int mini_prtmsg(m_source, m_ptr)
int m_source;
message *m_ptr;
{
	message m, *mp;

	mp = &m;
	/*	CopyMess(proc_nr(proc_ptr), proc_ptr, m_ptr, proc_addr(HARDWARE), mp);		*/
	CopyMess_ep(proc_ptr->p_endpoint, proc_ptr, m_ptr, proc_addr(HARDWARE), mp);
	mp->m_source = m_source;
	MHDEBUG("prtmsg: proc=%s vmid=%d " MSG1_FORMAT, proc_ptr->p_name, proc_ptr->p_vmid , MSG1_FIELDS(mp));

	return(OK);
}

/*===========================================================================*
 *				raw_notify				     *
 *===========================================================================*/
PUBLIC int raw_notify(src_p, dst_p)
int src_p;			/* sender of the notification */
int dst_p;			/* who is to be notified */
{
	struct proc *src_ptr, *dst_ptr;
	int rcode;

	src_ptr = proc_addr(src_p);
	dst_ptr = proc_addr(dst_p);
#ifdef LANCE_DBG	
	if( dst_ptr->p_endpoint == lance_ep ) {
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(dst_ptr));
	}
#endif	
	rcode = MH_notify(src_ptr, dst_p);
	if(rcode ) {
		panic("raw_notify: src_p",src_p);
	}
	return(rcode);
}

/*===========================================================================*
 *				notify_proxy				     *
 * Send a NOTIFY message to VM proxy server on VM0			     *
 *===========================================================================*/
PRIVATE int notify_proxy(int driver_id)
{
	struct proc *px;
	int proxy_ep, proxy_nr, saved_vm, rcode;

	proxy_ep = vmm_desc(running_vm).vmm_endpoint;
	proxy_nr = _ENDPOINT_P(proxy_ep);
#define IPC_DBG	
#ifdef IPC_DBG
	MHDEBUG("notify_proxy: proxy_ep=%d proxy_nr=%d driver_id=%d\n",
		proxy_ep, proxy_nr, driver_id);
#endif /* IPC_DBG */

	px = proc_addr_vm(VM0, proxy_nr);
	if( priv(px)->s_dr_bitmap & (1 << driver_id) ) {
		priv(px)->s_int_pending = (1 << driver_id);
#ifdef IPC_DBG
		MHDEBUG(PROC_FORMAT, PROC_FIELDS(px));
		MHDEBUG(PRIV_FORMAT, PRIV_FIELDS(priv(px)));
#endif /* IPC_DBG */
		saved_vm = running_vm;
		running_vm = VM0;
		rcode = MH_notify(proc_addr_vm(VM0,HARDWARE), proxy_nr);
		running_vm = saved_vm;
	} else {
		rcode = EACCES;
	}
	if(rcode) {
		MHRETURN(rcode);
	}
	return(rcode);
}
#endif /*  MHYPER */

