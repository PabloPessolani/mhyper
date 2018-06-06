/* This task handles the interface between the kernel and user-level servers.
 * System services can be accessed by doing a system call. System calls are
 * transformed into request messages, which are handled by this task. By
 * convention, a sys_call() is transformed in a SYS_CALL request message that
 * is handled in a function named do_call().
 *
 * A private call vector is used to map all system calls to the functions that
 * handle them. The actual handler functions are contained in separate files
 * to keep this file clean. The call vector is used in the system task's main
 * loop to handle all incoming requests.
 *
 * In addition to the main sys_task() entry point, which starts the main loop,
 * there are several other minor entry points:
 *   get_priv:		assign privilege structure to user or system process
 *   send_sig:		send a signal directly to a system process
 *   cause_sig:		take action to cause a signal to occur via PM
 *   umap_local:	map virtual address in LOCAL_SEG to physical
 *   umap_remote:	map virtual address in REMOTE_SEG to physical
 *   umap_bios:		map virtual address in BIOS_SEG to physical
 *   virtual_copy:	copy bytes from one virtual address to another
 *   get_randomness:	accumulate randomness in a buffer
 *   clear_endpoint:	remove a process' ability to send and receive messages
 *
 * Changes:
 *   Aug 04, 2005   check if system call is allowed  (Jorrit N. Herder)
 *   Jul 20, 2005   send signal to services with message  (Jorrit N. Herder)
 *   Jan 15, 2005   new, generalized virtual copy function  (Jorrit N. Herder)
 *   Oct 10, 2004   dispatch system calls from call vector  (Jorrit N. Herder)
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 */

#include "kernel.h"
#include "system.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/sigcontext.h>
#include <minix/endpoint.h>
#if (CHIP == INTEL)
#include <ibm/memory.h>
#include "protect.h"
#endif

/* Declaration of the call vector that defines the mapping of system calls
 * to handler functions. The vector is initialized in sys_init() with map(),
 * which makes sure the system call numbers are ok. No space is allocated,
 * because the dummy is declared extern. If an illegal call is given, the
 * array size will be negative and this won't compile.
 */
PUBLIC int (*call_vec[NR_SYS_CALLS])(message *m_ptr);

#define map(call_nr, handler) \
    {extern int dummy[NR_SYS_CALLS>(unsigned)(call_nr-KERNEL_CALL) ? 1:-1];} \
    call_vec[(call_nr-KERNEL_CALL)] = (handler);

/*    kprintf("call_nr=%X handler=%X\n",call_nr, (handler))	*/

FORWARD _PROTOTYPE( void initialize, (void));


#define FAULT_VM_LIMITS(v,a,s)  (a < vm_hyper[v].vm_start) ||  	\
	    ((a) > vm_hyper[v].vm_end)||  	\
	    ((a+s) > vm_hyper[v].vm_end)
#ifdef ANTERIOR
#define FAULT_PROC_LIMITS(a,p,s) 	 (a < p->p_memmap[D].mem_phys) ||				\
	    (a > (p->p_memmap[D].mem_phys+(p->p_memmap[D].mem_len<<CLICK_SHIFT))) ||  	\
	    ((a+s) > (p->p_memmap[D].mem_phys+(p->p_memmap[D].mem_len<<CLICK_SHIFT)))
#endif /* ANTERIOR */

#define FAULT_PROC_LIMITS(a,p,s) 	 ((a < (p->p_memmap[D].mem_phys<<CLICK_SHIFT)) ||\
	    (a > ((p->p_memmap[D].mem_phys+p->p_memmap[D].mem_len)<<CLICK_SHIFT)) ||  	\
	    ((a+s) > ((p->p_memmap[D].mem_phys+p->p_memmap[D].mem_len)<<CLICK_SHIFT)))

/* #define IPC_DBG 1 */
#define PRINTER_DBG 1

/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
PUBLIC void sys_task()
{
        /* Main entry point of sys_task.  Get the message and dispatch on type. */
        static message m;
        register int result;
        register struct proc *caller_ptr;
        unsigned int call_nr;
        int rcode;
#ifdef MHYPER
        message *m_ptr;
#endif /*  MHYPER */

        /* Initialize the system task. */
        initialize();

        while (TRUE) {
                /* Get work. Block and wait until a request message arrives. */
                receive(ANY, &m);
#ifdef IPC_DBG
				if( running_vm > VM0 ) {
					m_ptr = &m;
					MHDEBUG("SYSTEM: running_vm=%d "MSG1_FORMAT, running_vm, MSG1_FIELDS(m_ptr));
                }
#endif /* IPC_DBG */						

                call_nr = (unsigned) m.m_type - KERNEL_CALL;
                who_e = m.m_source;
                okendpt(who_e, &who_p);

#ifdef PRINTER_DBG
                if(who_e == 35596 && running_vm > VM0)
					MHDEBUG("SYSTEM: PRINTER running_vm=%d " MSG1_FORMAT, running_vm, MSG1_FIELDS(m_ptr));
#endif /* PRINTER_DBG */				
				
#ifdef AT_WINI_DBG
                if(who_e == 35555)
                        MHDEBUG("SYSTEM: who_e=%d who_p=%d\n",who_e, who_p);
#endif /* AT_WINI_DBG */				

                /* See if the caller made a valid request and try to handle it. */
                caller_ptr = proc_addr(who_p);

#ifndef MHYPER  /* ANULADO TEMPORALMENTE HASTA SOLUCIONAR EL TEMA DE LA MASCARA DE CALLS */
                if (! (priv(caller_ptr)->s_call_mask & (1<<call_nr))) {
                        MHDEBUG("SYSTEM: ECALLDENIED who_e=%d call_nr=%d\n",who_e, call_nr );

#if DEBUG_ENABLE_IPC_WARNINGS
                        kprintf("SYSTEM: request %d from %d denied.\n", call_nr,m.m_source);
#endif
                        result = ECALLDENIED;			/* illegal message type */
                }	  else
#endif /*-------------------------- MHYPER --------------------------------------- */

                        if (call_nr >= NR_SYS_CALLS) {		/* check call number */
#if DEBUG_ENABLE_IPC_WARNINGS
                                kprintf("SYSTEM: illegal request %d from %d.\n", call_nr,m.m_source);
#endif
                                result = EBADREQUEST;			/* illegal message type */
                        } else {
#ifdef ANULADO
								if( (call_nr+KERNEL_CALL) == SYS_VCOPYVM ) 
									MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif
						        result = (*call_vec[call_nr])(&m);	/* handle the system call */
                        }

                /* Send a reply, unless inhibited by a handler function. Use the kernel
                 * function lock_send() to prevent a system call trap. The destination
                 * is known to be blocked waiting for a message.
                 */
				rcode = OK;
                if (result != EDONTREPLY) {
                        m.m_type = result;			/* report status of call */
						if( (call_nr+KERNEL_CALL) == SYS_VCOPYVM ) {
							m_ptr = &m;
#ifdef ANULADO
							MHDEBUG("SYSTEM: SYS_VCOPYVM running_vm=%d "MSG5_FORMAT, running_vm, MSG5_FIELDS(m_ptr));
							MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
#endif
						}
						if (OK != (rcode =lock_send(m.m_source, &m))) {
                            kprintf("SYSTEM, reply to call_nr=%d running_vm=%d ep=%d failed: %d\n", call_nr, running_vm, m.m_source, rcode);
							MHDEBUG("SYSTEM: result=%d running_vm=%d rcode=%d "MSG5_FORMAT, result, running_vm, rcode, MSG5_FIELDS(m_ptr));							
								
                        }
				}
        }
}

/*===========================================================================*
 *				initialize				     *
 *===========================================================================*/
PRIVATE void initialize(void)
{
        register struct priv *sp;
        int i;

#ifdef MHYPER
        int vmid;

        systask_ptr->p_vmid = VM0;

        running_vm= VM0;
		
        MHDEBUG("SYSTEM: name=%s vmid=%d\n", systask_ptr->p_name,systask_ptr->p_vmid);
		
        for(vmid=0; vmid < NR_VMS; vmid++) {
                running_vm = vmid;		/*running_vm is used by macros!! */
                /* Initialize all alarm timers for all processes. */
                for (sp=BEG_PRIV_ADDR; sp < END_PRIV_ADDR; sp++) {
                        tmr_inittimer(&(sp->s_alarm_timer));
                }
        }
        running_vm= VM0;

        /* Initialize IRQ handler hooks. Mark all hooks available. */
        for (i=0; i<NR_IRQ_HOOKS; i++) {
                irq_hooks[i].proc_nr_e = NONE;
                irq_hooks[i].vmid = VM0;
                irq_hooks[i].id = i;
        }
#else /*  MHYPER */
        /* Initialize all alarm timers for all processes. */
        for (sp=BEG_PRIV_ADDR; sp < END_PRIV_ADDR; sp++) {
                tmr_inittimer(&(sp->s_alarm_timer));
        }

        /* Initialize IRQ handler hooks. Mark all hooks available. */
        for (i=0; i<NR_IRQ_HOOKS; i++) {
                irq_hooks[i].proc_nr_e = NONE;
        }
#endif /*  MHYPER */

        /* Initialize the call vector to a safe default handler. Some system calls
         * may be disabled or nonexistant. Then explicitely map known calls to their
         * handler functions. This is done with a macro that gives a compile error
         * if an illegal call number is used. The ordering is not important here.
         */
        for (i=0; i<NR_SYS_CALLS; i++) {
                call_vec[i] = do_unused;
        }

        /* Process management. */
        map(SYS_FORK, do_fork); 		/* a process forked a new process */
        map(SYS_EXEC, do_exec);		/* update process after execute */
        map(SYS_EXIT, do_exit);		/* clean up after process exit */
        map(SYS_NICE, do_nice);		/* set scheduling priority */
        map(SYS_PRIVCTL, do_privctl);		/* system privileges control */
        map(SYS_TRACE, do_trace);		/* request a trace operation */

        /* Signal handling. */
        map(SYS_KILL, do_kill); 		/* cause a process to be signaled */
        map(SYS_GETKSIG, do_getksig);		/* PM checks for pending signals */
        map(SYS_ENDKSIG, do_endksig);		/* PM finished processing signal */
        map(SYS_SIGSEND, do_sigsend);		/* start POSIX-style signal */
        map(SYS_SIGRETURN, do_sigreturn);	/* return from POSIX-style signal */

        /* Device I/O. */
        map(SYS_IRQCTL, do_irqctl);  		/* interrupt control operations */
        map(SYS_DEVIO, do_devio);   		/* inb, inw, inl, outb, outw, outl */
        map(SYS_SDEVIO, do_sdevio);		/* phys_insb, _insw, _outsb, _outsw */
        map(SYS_VDEVIO, do_vdevio);  		/* vector with devio requests */
        map(SYS_INT86, do_int86);  		/* real-mode BIOS calls */

        /* Memory management. */
        map(SYS_NEWMAP, do_newmap);		/* set up a process memory map */
        map(SYS_SEGCTL, do_segctl);		/* add segment and get selector */
        map(SYS_MEMSET, do_memset);		/* write char to memory area */
        map(SYS_VM_SETBUF, do_vm_setbuf); 	/* PM passes buffer for page tables */
        map(SYS_VM_MAP, do_vm_map); 		/* Map/unmap physical (device) memory */

        /* Copying. */
        map(SYS_UMAP, do_umap);		/* map virtual to physical address */
        map(SYS_VIRCOPY, do_vircopy); 	/* use pure virtual addressing */
        map(SYS_PHYSCOPY, do_physcopy); 	/* use physical addressing */
        map(SYS_VIRVCOPY, do_virvcopy);	/* vector with copy requests */
        map(SYS_PHYSVCOPY, do_physvcopy);	/* vector with copy requests */

        /* Clock functionality. */
        map(SYS_TIMES, do_times);		/* get uptime and process times */
        map(SYS_SETALARM, do_setalarm);	/* schedule a synchronous alarm */

        /* System control. */
        map(SYS_ABORT, do_abort);		/* abort MINIX */
        map(SYS_GETINFO, do_getinfo); 	/* request system information */
        map(SYS_IOPENABLE, do_iopenable); 	/* Enable I/O */

#ifdef MHYPER
        map(SYS_GETVMID, do_getvmid); 	/* Get process VMID */
        map(SYS_VMMREQ, do_vmmreq); 	/* Request from VMM */
        map(SYS_VCOPYVM, do_vcopyvm);	/* copy data between virtual address spaces inter VMs */
        map(SYS_UMAPVM, do_umapvm);		/* map virtual to physical address of a process on other VM */
        map(SYS_KILLVM, do_killvm); 	/* cause a process within a VM to be signaled */
#endif /*  MHYPER */


}

/*===========================================================================*
 *				get_priv				     *
 *===========================================================================*/
PUBLIC int get_priv(rc, proc_type)
register struct proc *rc;		/* new (child) process pointer */
int proc_type;				/* system or user process flag */
{
        /* Get a privilege structure. All user processes share the same privilege
         * structure. System processes get their own privilege structure.
         */
        register struct priv *sp;			/* privilege structure */

        if (proc_type == SYS_PROC) {			/* find a new slot */
                for (sp = BEG_PRIV_ADDR; sp < END_PRIV_ADDR; ++sp)
                        if (sp->s_proc_nr == NONE && sp->s_id != USER_PRIV_ID) break;
                if (sp->s_proc_nr != NONE) return(ENOSPC);
                rc->p_priv = sp;			/* assign new slot */
                rc->p_priv->s_proc_nr = proc_nr(rc);	/* set association */
		if( running_vm == VM0)
	                rc->p_priv->s_nr[VM0] = proc_nr(rc);	
                rc->p_priv->s_flags = SYS_PROC;		/* mark as privileged */

        } else {
                rc->p_priv = priv_addr(USER_PRIV_ID);	/* use shared slot */
                rc->p_priv->s_proc_nr = INIT_PROC_NR;	/* set association */
                rc->p_priv->s_flags = 0;			/* no initial flags */
        }

#ifdef MHYPER2
        MHDEBUG("get_priv p_name=%s p_nr=%d s_id=%d flags=%X p_vmid=%d s_vmid=%d\n",
                rc->p_name, rc->p_nr, sp->s_id, rc->p_priv->s_flags, rc->p_vmid, sp->s_vmid);
#endif /*  MHYPER2 */

        return(OK);
}

/*===========================================================================*
 *				get_randomness				     *
 *===========================================================================*/
PUBLIC void get_randomness(source)
int source;
{
        /* On machines with the RDTSC (cycle counter read instruction - pentium
         * and up), use that for high-resolution raw entropy gathering. Otherwise,
         * use the realtime clock (tick resolution).
         *
         * Unfortunately this test is run-time - we don't want to bother with
         * compiling different kernels for different machines.
         *
         * On machines without RDTSC, we use read_clock().
         */
        int r_next;
        unsigned long tsc_high, tsc_low;

        source %= RANDOM_SOURCES;
        r_next= krandom.bin[source].r_next;
        if (machine.processor > 486) {
                read_tsc(&tsc_high, &tsc_low);
                krandom.bin[source].r_buf[r_next] = tsc_low;
        } else {
                krandom.bin[source].r_buf[r_next] = read_clock();
        }
        if (krandom.bin[source].r_size < RANDOM_ELEMENTS) {
                krandom.bin[source].r_size ++;
        }
        krandom.bin[source].r_next = (r_next + 1 ) % RANDOM_ELEMENTS;
}

/*===========================================================================*
 *				send_sig				     *
 *===========================================================================*/
PUBLIC void send_sig(int proc_nr, int sig_nr)
{
        /* Notify a system process about a signal. This is straightforward. Simply
         * set the signal that is to be delivered in the pending signals map and
         * send a notification with source SYSTEM.
         *
         * Process number is verified to avoid writing in random places, but we
         * don't kprintf() or panic() because that causes send_sig() invocations.
         */
        register struct proc *rp;
        static int n;
#ifdef MHYPER
        int saved_vm;

        lock(9,"send_sig");
        rp = proc_addr(proc_nr);
        if(!isokprocn(proc_nr) || isemptyp(rp)) {
                unlock(9);
                return;
        }
        sigaddset(&priv(rp)->s_sig_pending, sig_nr);
        saved_vm 	= running_vm;
        running_vm	= rp->p_vmid;
        raw_notify(SYSTEM, rp->p_nr);
        /* Restore  vmids*/
        running_vm 	=saved_vm;
        unlock(9);

#else /* MYPER */
        if(!isokprocn(proc_nr) || isemptyn(proc_nr))
                return;
        rp = proc_addr(proc_nr);
        sigaddset(&priv(rp)->s_sig_pending, sig_nr);
        lock_notify(SYSTEM, rp->p_endpoint);
#endif /*  MHYPER */
}

/*===========================================================================*
 *				cause_sig				     *
 *===========================================================================*/
PUBLIC void cause_sig(proc_nr, sig_nr)
int proc_nr;			/* process to be signalled */
int sig_nr;			/* signal to be sent, 1 to _NSIG */
{
        /* A system process wants to send a signal to a process.  Examples are:
         *  - HARDWARE wanting to cause a SIGSEGV after a CPU exception
         *  - TTY wanting to cause SIGINT upon getting a DEL
         *  - FS wanting to cause SIGPIPE for a broken pipe
         * Signals are handled by sending a message to PM.  This function handles the
         * signals and makes sure the PM gets them by sending a notification. The
         * process being signaled is blocked while PM has not finished all signals
         * for it.
         * Race conditions between calls to this function and the system calls that
         * process pending kernel signals cannot exist. Signal related functions are
         * only called when a user process causes a CPU exception and from the kernel
         * process level, which runs to completion.
         */
        register struct proc *rp;

        /* Check if the signal is already pending. Process it otherwise. */
        rp = proc_addr(proc_nr);

#ifdef MHYPER
        MHDEBUG("cause_sig: proc_nr=%d, sig_nr=%d running_vm=%d\n",proc_nr, sig_nr, running_vm);
#endif /*  MHYPER */

        if (! sigismember(&rp->p_pending, sig_nr)) {
                sigaddset(&rp->p_pending, sig_nr);
                if (! (rp->p_rts_flags & SIGNALED)) {		/* other pending */
                        if (rp->p_rts_flags == 0) lock_dequeue(rp);	/* make not ready */
                        rp->p_rts_flags |= SIGNALED | SIG_PENDING;	/* update flags */
                        send_sig(PM_PROC_NR, SIGKSIG);
                }
        }
}


/*===========================================================================*
 *				umap_bios				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_bios(rp, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
vir_bytes vir_addr;		/* virtual address in BIOS segment */
vir_bytes bytes;		/* # of bytes to be copied */
{
        /* Calculate the physical memory address at the BIOS. Note: currently, BIOS
         * address zero (the first BIOS interrupt vector) is not considered, as an
         * error here, but since the physical address will be zero as well, the
         * calling function will think an error occurred. This is not a problem,
         * since no one uses the first BIOS interrupt vector.
         */

        /* Check all acceptable ranges. */
        if (vir_addr >= BIOS_MEM_BEGIN && vir_addr + bytes <= BIOS_MEM_END)
                return (phys_bytes) vir_addr;
        else if (vir_addr >= BASE_MEM_TOP && vir_addr + bytes <= UPPER_MEM_END)
                return (phys_bytes) vir_addr;

#if DEAD_CODE	/* brutal fix, if the above is too restrictive */
        if (vir_addr >= BIOS_MEM_BEGIN && vir_addr + bytes <= UPPER_MEM_END)
                return (phys_bytes) vir_addr;
#endif

        kprintf("Warning, error in umap_bios, virtual address 0x%x\n", vir_addr);
        return 0;
}

/*===========================================================================*
 *				umap_local				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_local(rp, seg, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
int seg;			/* T, D, or S segment */
vir_bytes vir_addr;		/* virtual address in bytes within the seg */
vir_bytes bytes;		/* # of bytes to be copied */
{
        /* Calculate the physical memory address for a given virtual address. */
        vir_clicks vc;		/* the virtual address in clicks */
        phys_bytes pa;		/* intermediate variables as phys_bytes */
#if (CHIP == INTEL)
        phys_bytes seg_base;
#endif

        /* If 'seg' is D it could really be S and vice versa.  T really means T.
         * If the virtual address falls in the gap,  it causes a problem. On the
         * 8088 it is probably a legal stack reference, since "stackfaults" are
         * not detected by the hardware.  On 8088s, the gap is called S and
         * accepted, but on other machines it is called D and rejected.
         * The Atari ST behaves like the 8088 in this respect.
         */
#ifdef MHYPER
        int proxy_ep, proxy_nr;

        if( running_vm != VM0) {
                if( rp->p_misc_flags & PROMICUOUS_TASK) {
                        rp = proc_addr_vm(VM0, rp->p_nr);
                        MHDEBUG("umap_local	PROMICUOUS_TASK %s p_nr=%d\n", rp->p_name, rp->p_nr);
                } else if (rp->p_misc_flags & VIRTUAL_TASK) {
                        proxy_ep =  vm_hyper[running_vm].vm_VMM.vmm_endpoint;
                        isokendpt_vm(VM0, proxy_ep, &proxy_nr);
                        rp = proc_addr_vm(VM0, proxy_nr);
                        MHDEBUG("umap_local	VIRTUAL_TASK %s p_nr=%d\n", rp->p_name, rp->p_nr);
                }
        }
#endif /*  MHYPER */

        if (bytes <= 0) return( (phys_bytes) 0);
        if (vir_addr + bytes <= vir_addr) return 0;	/* overflow */
        vc = (vir_addr + bytes - 1) >> CLICK_SHIFT;	/* last click of data */

#if (CHIP == INTEL) || (CHIP == M68000)
        if (seg != T)
                seg = (vc < rp->p_memmap[D].mem_vir + rp->p_memmap[D].mem_len ? D : S);
#else
        if (seg != T)
                seg = (vc < rp->p_memmap[S].mem_vir ? D : S);
#endif

        if ((vir_addr>>CLICK_SHIFT) >= rp->p_memmap[seg].mem_vir +
            rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );

        if (vc >= rp->p_memmap[seg].mem_vir +
            rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );

#if (CHIP == INTEL)
        seg_base = (phys_bytes) rp->p_memmap[seg].mem_phys;
        seg_base = seg_base << CLICK_SHIFT;	/* segment origin in bytes */
#endif
        pa = (phys_bytes) vir_addr;
#if (CHIP != M68000)
        pa -= rp->p_memmap[seg].mem_vir << CLICK_SHIFT;
        return(seg_base + pa);
#endif
#if (CHIP == M68000)
        pa -= (phys_bytes)rp->p_memmap[seg].mem_vir << CLICK_SHIFT;
        pa += (phys_bytes)rp->p_memmap[seg].mem_phys << CLICK_SHIFT;
        return(pa);
#endif
}

/*===========================================================================*
 *				umap_remote				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_remote(rp, seg, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
int seg;			/* index of remote segment */
vir_bytes vir_addr;		/* virtual address in bytes within the seg */
vir_bytes bytes;		/* # of bytes to be copied */
{
        /* Calculate the physical memory address for a given virtual address. */
        struct far_mem *fm;

        if (bytes <= 0) return( (phys_bytes) 0);
        if (seg < 0 || seg >= NR_REMOTE_SEGS) return( (phys_bytes) 0);

        fm = &rp->p_priv->s_farmem[seg];
        if (! fm->in_use) return( (phys_bytes) 0);
        if (vir_addr + bytes > fm->mem_len) return( (phys_bytes) 0);

        return(fm->mem_phys + (phys_bytes) vir_addr);
}

/* #define ANULADO 1 */
/*===========================================================================*
 *				virtual_copy				     *
 * MHYPER: if one of the process is a PROMISCOUS_TASK its address space	     *
 * is used for the copy 						     *
 *===========================================================================*/
PUBLIC int virtual_copy(src_addr, dst_addr, bytes)
struct vir_addr *src_addr;	/* source virtual address */
struct vir_addr *dst_addr;	/* destination virtual address */
vir_bytes bytes;		/* # of bytes to copy  */
{
        /* Copy bytes from virtual address src_addr to virtual address dst_addr.
         * Virtual addresses can be in ABS, LOCAL_SEG, REMOTE_SEG, or BIOS_SEG.
         */
        struct vir_addr *vir_addr[2];	/* virtual source and destination address */
        phys_bytes phys_addr[2];	/* absolute source and destination */
        int seg_index;
        int i, type[2], vmid;
        struct proc *p[2], *caller_ptr, *p_ptr;
        int proc_nr[2];
        vmm_desc_t *vmm_ptr;
        struct vm_struct *vm_ptr;
		struct mem_map *mm_ptr;


        /* Check copy count. */
        if (bytes <= 0) return(EDOM);
        caller_ptr = get_proc_addr(who_p);

        /* Do some more checks and map virtual addresses to physical addresses. */
        vir_addr[_SRC_] = src_addr;
        vir_addr[_DST_] = dst_addr;
        for (i=_SRC_; i<=_DST_; i++) {

                type[i] = vir_addr[i]->segment & SEGMENT_TYPE;

#ifdef MHYPER
                if(type[i] != PHYS_SEG && isokendpt(vir_addr[i]->proc_nr_e, &proc_nr[i])) {
                        p[i] = proc_addr(proc_nr[i]);
                        vmid = p[i]->p_vmid;
                        if( vmid != VM0 ) {
                                if( vm_hyper[vmid].vm_flags != (VM_LOADED | VM_RUNNING) )
                                        MHERROR("SYSTEM: virtual_copy: bad VM status:%d\n",EVMSTATUS);
                                if(p[i]->p_misc_flags & PROMICUOUS_TASK) {
                                        MHDEBUG("PROMICUOUS_TASK virtual_copy[%d] ep=%d proc_nr=%d bytes=%d\n",
                                                i, vir_addr[i]->proc_nr_e, proc_nr[i], bytes);
                                        p[i] = p[i]->p_promiscuous; /* proc_addr_vm(VM0, proc_nr[i]); */
                                        if( !(priv(p[i])->s_vm_bitmap & (1 << vmid)))
                                            MHERROR("SYSTEM: PROMICUOUS_TASK virtual_copy bad VMID:%d\n",EBADVMID);
                                } else if (p[i]->p_misc_flags & VIRTUAL_TASK) {
                                        MHDEBUG("VIRTUAL_TASK virtual_copy[%d] ep=%d proc_nr=%d bytes=%d\n",
                                                i, vir_addr[i]->proc_nr_e, proc_nr[i], bytes);
                                        vmm_ptr =&vmm_desc(vmid);
                                        if (!isokendpt_vm(VM0, vmm_ptr->vmm_endpoint, &proc_nr[i]))
                                                MHERROR("SYSTEM: virtual_copy: bad endpoint:%d\n",EINVAL);
                                        p[i] = proc_addr_vm(VM0, proc_nr[i]);
                                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[i]));
                                        if( p[i]->p_proxy != vmid )
                                            MHERROR("SYSTEM: VIRTUAL_TASK virtual_copy bad VMID:%d\n",EBADVMID);
                                }
                        }
                } else {
#ifdef ANULADO			
					MHDEBUG("SYSTEM: virtual_copy: i=%d endp=%d running_vm=%d\n",i, vir_addr[i]->proc_nr_e,running_vm);
#endif /* ANULADO */				
                    p[i] = NULL;
                }
#else /*  MHYPER */
                if(type != PHYS_SEG && isokendpt(vir_addr[i]->proc_nr_e, &proc_nr))
                        p = proc_addr(proc_nr);
                else
                        p = NULL;
#endif /*  MHYPER */

                /* Get physical address. */
                switch(type[i]) {
                case LOCAL_SEG:
                        if(!p[i]) return EDEADSRCDST;
                        seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
                        phys_addr[i] = umap_local(p[i], seg_index, vir_addr[i]->offset, bytes);
                        break;
                case REMOTE_SEG:
                        if(!p[i]) return EDEADSRCDST;
                        seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
                        phys_addr[i] = umap_remote(p[i], seg_index, vir_addr[i]->offset, bytes);
                        break;
                case BIOS_SEG:
                        if(!p[i]) return EDEADSRCDST;
                        phys_addr[i] = umap_bios(p[i], vir_addr[i]->offset, bytes );
                        break;
                case PHYS_SEG:
                        phys_addr[i] = vir_addr[i]->offset;
                        break;
                default:
                        return(EINVAL);
                }

                /* Check if mapping succeeded. */
                if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG)
                        return(EFAULT);
        }

        /*
        * CHECK ADDRESS RANGE AGAINST VM LIMITS OR PROXY/PROMISCUOS PROCESSES MEMORY LIMITS
        * The caller must be from a VM != VM0
        * and the copy must be LOCAL_SEG->LOCAL_SEG
        */

    if( (caller_ptr->p_vmid != VM0) && 
			(type[_SRC_] == LOCAL_SEG &&  type[_DST_] == LOCAL_SEG) ) {
                /*
				* copy between address spaces of processes of the same VM
				*/
#ifdef ANULADO
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_SRC_]));
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
                vm_ptr = &vm_hyper[p[_DST_]->p_vmid];
                MHDEBUG(VM_FORMAT, VM_FIELDS(vm_ptr));
                MHDEBUG("SYSTEM: virtual_copy: phys_src=%X  phys_dst=%X bytes=%d\n",
                        phys_addr[_SRC_], phys_addr[_DST_], bytes);
#endif /* ANULADO */
                if( p[_SRC_]->p_vmid == p[_DST_]->p_vmid) {
                        /* both must be within VM address limits */
                        if( FAULT_VM_LIMITS(p[_SRC_]->p_vmid,phys_addr[_SRC_], bytes))
                                MHERROR("SYSTEM: virtual_copy: src vm limits:%d\n",EFAULT);
                        if( FAULT_VM_LIMITS(p[_DST_]->p_vmid,phys_addr[_DST_], bytes))
                                MHERROR("SYSTEM: virtual_copy: dst vm limits:%d\n",EFAULT);
                        /*
                        * copy from VMx -> VM0(proxy/promiscuous process)
                        */

                } else if (  p[_SRC_]->p_vmid != VM0 && p[_DST_]->p_vmid == VM0) {
#ifdef ANULADO
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_SRC_]));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
#endif /* ANULADO */
                        /* check source address limits */
                        if( FAULT_VM_LIMITS(p[_SRC_]->p_vmid,phys_addr[_SRC_], bytes))
                                MHERROR("SYSTEM: virtual_copy: src vm limits:%d\n",EFAULT);

                        /* get the destination descriptor on VMx */
						/* p_ptr = proc_addr_vm(p[_SRC_]->p_vmid, proc_nr[_DST_]); */

                        if(p[_DST_]->p_misc_flags & PROXY_SERVER) {
                            /* check if the destination proxy server is the proxy of VMx */
                            if(p[_DST_]->p_proxy != p[_SRC_]->p_vmid)
                                MHERROR("SYSTEM: virtual_copy: not vm proxy:%d\n",EFAULT);

                            /* destination address must be within a PROXY process address space  */
                            if( FAULT_PROC_LIMITS(phys_addr[_DST_],p[_DST_],bytes))
                                MHERROR("SYSTEM: virtual_copy: dst proc limits:%d\n",EFAULT);
                        } else {
#ifdef ANULADO										
                            if (p_ptr->p_misc_flags & PROMICUOUS_TASK) {
                                if( FAULT_PROC_LIMITS(phys_addr[_DST_],p[_DST_],bytes))
                                    MHERROR("SYSTEM: virtual_copy: dst PROMICUOUS_TASK limits:%d\n",EFAULT);
                            } else {
								MHERROR("SYSTEM: virtual_copy: not promiscuous:%d\n",EFAULT);
                            }
#else /* ANULADO */

							if( FAULT_PROC_LIMITS(phys_addr[_DST_],p[_DST_],bytes)){
		                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
								MHDEBUG("DST=%d vmid=%d phys_addr=%X bytes=%d CLICK_SHIFT=%d\n", 
									p[_DST_]->p_endpoint, 
									p[_DST_]->p_vmid,
									phys_addr[_DST_], 
									bytes,
									CLICK_SHIFT);
								mm_ptr = &p[_DST_]->p_memmap[D];
								MHDEBUG( MMAP_FORMAT, MMAP_FIELDS(mm_ptr)); 
								MHDEBUG("LOW_ADDR=%X HIGH_ADDR=%X\n",
									p[_DST_]->p_memmap[D].mem_phys,
									(p[_DST_]->p_memmap[D].mem_phys+(p[_DST_]->p_memmap[D].mem_len<<CLICK_SHIFT))); 
                                MHERROR("SYSTEM: virtual_copy: dst proc limits:%d\n",EFAULT);
							}
#endif /* ANULADO */
                        }
                } else if (  p[_SRC_]->p_vmid == VM0 && p[_DST_]->p_vmid != VM0) {
#ifdef ANULADO
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_SRC_]));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
#endif /* ANULADO */

                        /* check destination address limits */
                        if( FAULT_VM_LIMITS(p[_DST_]->p_vmid,phys_addr[_DST_], bytes))
                                MHERROR("SYSTEM: virtual_copy: dst vm limits:%d\n",EFAULT);

                        /* get the source descriptor on VMx */
                        /* p_ptr = proc_addr_vm(p[_DST_]->p_vmid, proc_nr[_SRC_]); */

                        if(p[_SRC_]->p_misc_flags & PROXY_SERVER) {
                            /* check if the destination proxy server is the proxy of VMx */
                            if(p[_SRC_]->p_proxy != p[_DST_]->p_vmid)
                                MHERROR("SYSTEM: virtual_copy: not vm proxy:%d\n",EFAULT);

                            /* destination address must be within a PROXY process address space  */
                            if(FAULT_PROC_LIMITS(phys_addr[_SRC_],p[_SRC_],bytes))
                                MHERROR("SYSTEM: virtual_copy: dst proc limits:%d\n",EFAULT);
                        } else {
#ifdef ANULADO																
                            if (p_ptr->p_misc_flags & PROMICUOUS_TASK) {
                                if( FAULT_PROC_LIMITS(phys_addr[_SRC_],p[_SRC_],bytes))
                                    MHERROR("SYSTEM: virtual_copy: prom proc limits:%d\n",EFAULT);
                            } else {
                                MHERROR("SYSTEM: virtual_copy: bad dst:%d\n",EFAULT);
                            }
#else /* ANULADO */
				            if( FAULT_PROC_LIMITS(phys_addr[_SRC_],p[_SRC_],bytes))
                                MHERROR("SYSTEM: virtual_copy: prom proc limits:%d\n",EFAULT);			
#endif /* ANULADO */
                       }
                        /*
						* copy from VMx -> VMy !!!!!!!
						*/
                } else {
                        MHERROR("SYSTEM: virtual_copy: can copy on VM0:%d\n",EFAULT);
                }
	}

    phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);

    return(OK);
}



/*===========================================================================*
 *			         clear_endpoint				     *
 *===========================================================================*/
PUBLIC void clear_endpoint(rc)
register struct proc *rc;		/* slot of process to clean up */
{
        register struct proc *rp;		/* iterate over process table */
        register struct proc **xpp;		/* iterate over caller queue */
#ifdef IPC_DBG
        if(rc->p_vmid != VM0) {
                MHDEBUG("clear_endpoint running_vm=%d\n",running_vm);
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(rc));
        }
#endif /*  IPC_DBG */

        if(isemptyp(rc)) panic("clear_endpoint: empty process", proc_nr(rc));

        /* Make sure that the exiting process is no longer scheduled. */
        if (rc->p_rts_flags == 0) lock_dequeue(rc);
        rc->p_rts_flags |= NO_ENDPOINT;

        /* If the process happens to be queued trying to send a
         * message, then it must be removed from the message queues.
         */
        if (rc->p_rts_flags & SENDING) {
                int target_proc;

                okendpt(rc->p_sendto_e, &target_proc);
                xpp = &proc_addr(target_proc)->p_caller_q; /* destination's queue */
                while (*xpp != NIL_PROC) {		/* check entire queue */
                        if (*xpp == rc) {			/* process is on the queue */
                                *xpp = (*xpp)->p_q_link;		/* replace by next process */
#if DEBUG_ENABLE_IPC_WARNINGS
                                kprintf("Proc %d removed from queue at %d\n",
                                        proc_nr(rc), rc->p_sendto_e);
#endif
                                break;				/* can only be queued once */
                        }
                        xpp = &(*xpp)->p_q_link;		/* proceed to next queued */
                }
                rc->p_rts_flags &= ~SENDING;
        }
        rc->p_rts_flags &= ~RECEIVING;

        /* Likewise, if another process was sending or receive a message to or from
         * the exiting process, it must be alerted that process no longer is alive.
         * Check all processes.
         */
#ifdef MHYPER
        if (rc->p_misc_flags & PROXY_SERVER) {
                rc->p_misc_flags &= ~PROXY_SERVER;
                rc->p_proxy = VM0;
        }
        rc->p_misc_flags &= ~(VMID_PENDING|VIRTUAL_TASK|PROMICUOUS_TASK|NOT_GETFROM);
        rc->p_recv_vm = ANY_VM;

        for (rp = vect_proc_addr(HARDWARE+1); rp < END_PROC_ADDR; rp++) {
#else /*  MHYPER */
        for (rp = BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
#endif /*  MHYPER */

                if(isemptyp(rp))
                        continue;

                /* Unset pending notification bits. */
                unset_sys_bit(priv(rp)->s_notify_pending, priv(rc)->s_id);
                priv(rp)->s_ntfy_cnt[priv(rc)->s_id] = 0; /* itinialize notify pending count */

                /* Check if process is receiving from exiting process. */
                if ((rp->p_rts_flags & RECEIVING) && rp->p_getfrom_e == rc->p_endpoint) {
                        rp->p_reg.retreg = ESRCDIED;		/* report source died */
                        rp->p_rts_flags &= ~RECEIVING;	/* no longer receiving */
#if DEBUG_ENABLE_IPC_WARNINGS
                        kprintf("Proc %d receive dead src %d\n", proc_nr(rp), proc_nr(rc));
#endif
                        if (rp->p_rts_flags == 0) lock_enqueue(rp);/* let process run again */
                }
                if ((rp->p_rts_flags & SENDING) && rp->p_sendto_e == rc->p_endpoint) {
                        rp->p_reg.retreg = EDSTDIED;		/* report destination died */
                        rp->p_rts_flags &= ~SENDING;		/* no longer sending */
#if DEBUG_ENABLE_IPC_WARNINGS
                        kprintf("Proc %d send dead dst %d\n", proc_nr(rp), proc_nr(rc));
#endif
                        if (rp->p_rts_flags == 0) lock_enqueue(rp);/* let process run again */
                }
        }
}


#ifdef MHYPER

/*===========================================================================*
 *				virtual_copy_vm				     *
 * Lets a PROXY_SERVER or PROMISCOUS_TASK to copy data from to a process     *
 * in a VM != VM0 							     *
 *===========================================================================*/
PUBLIC int virtual_copy_vm(src_addr, dst_addr, bytes)
struct vir_addr_vm *src_addr;	/* source virtual address */
struct vir_addr_vm *dst_addr;	/* destination virtual address */
vir_bytes bytes;		/* # of bytes to copy  */
{
        /* Copy bytes from virtual address src_addr to virtual address dst_addr.
         * Virtual addresses can be in ABS, LOCAL_SEG, REMOTE_SEG, or BIOS_SEG.
         */
        struct vir_addr_vm *vir_addr[2];	/* virtual source and destination address */
        phys_bytes phys_addr[2];	/* absolute source and destination */
        int i, type[2], vmid;
        struct proc *p[2], *caller_ptr, *p_ptr;
        int seg_index;
        int proc_nr[2];
        vmm_desc_t *vmm_ptr;
        struct vm_struct *vm_ptr;
 
        /* Check copy count. */
        if (bytes <= 0) return(EDOM);
        caller_ptr = get_proc_addr(who_p);
		
        /* Do some more checks and map virtual addresses to physical addresses. */
        vir_addr[_SRC_] = src_addr;
        vir_addr[_DST_] = dst_addr;
        for (i=_SRC_; i<=_DST_; i++) {
                type[i] = vir_addr[i]->segment & SEGMENT_TYPE;
				
				if(type[i] != PHYS_SEG && isokendpt_vm(vir_addr[i]->vmid, vir_addr[i]->proc_nr_e, &proc_nr[i])) {
#ifdef ANULADO					
				    MHDEBUG("virtual_copy_vm1: i=%d vmid=%d endp=%d, proc_nr=%d type=%d\n",
					i, vir_addr[i]->vmid, vir_addr[i]->proc_nr_e, proc_nr[i], type[i]);
#endif
					p[i] = proc_addr_vm(vir_addr[i]->vmid, proc_nr[i]);
                    vmid = vir_addr[i]->vmid;
                    if( vmid != VM0 ) {
                        if( vm_hyper[vmid].vm_flags != (VM_LOADED | VM_RUNNING) )
                            MHERROR("SYSTEM: virtual_copy_vm: bad VM status:%d\n",EVMSTATUS);
                        if(p[i]->p_misc_flags & PROMICUOUS_TASK) {
							MHDEBUG("PROMICUOUS_TASK virtual_copy_vm[%d] ep=%d proc_nr=%d bytes=%d\n",
								i, vir_addr[i]->proc_nr_e, proc_nr[i], bytes);
                            p[i] = p[i]->p_promiscuous; /* proc_addr_vm(VM0, proc_nr[i]); */
                            if( !(priv(p[i])->s_vm_bitmap & (1 << vmid)))
								MHERROR("SYSTEM: PROMICUOUS_TASK virtual_copy_vm bad VMID:%d\n",EBADVMID);
                        } else if (p[i]->p_misc_flags & VIRTUAL_TASK) {
                            MHDEBUG("VIRTUAL_TASK virtual_copy_vm[%d] ep=%d proc_nr=%d bytes=%d\n",
                                    i, vir_addr[i]->proc_nr_e, proc_nr[i], bytes);
                            vmm_ptr =&vmm_desc(vmid);
                            if (!isokendpt_vm(VM0, vmm_ptr->vmm_endpoint, &proc_nr[i]))
                                MHERROR("SYSTEM: virtual_copy_vm: bad endpoint:%d\n",EINVAL);
                            p[i] = proc_addr_vm(VM0, proc_nr[i]);
                            MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[i]));
                            if( p[i]->p_proxy != vmid )
                                MHERROR("SYSTEM: VIRTUAL_TASK virtual_copy_vm bad VMID:%d\n",EBADVMID);
                        }
                    }
                } else {
					MHDEBUG("SYSTEM: virtual_copy_vm: i=%d endp=%d running_vm=%d\n",i, vir_addr[i]->proc_nr_e,running_vm);
                    p[i] = NULL;
                }

                /* Get physical address. */
                switch(type[i]) {
                case LOCAL_SEG:
                        if(!p[i]) return EDEADSRCDST;
                        seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
                        phys_addr[i] = umap_local(p[i], seg_index, vir_addr[i]->offset, bytes);
                        break;
                case REMOTE_SEG:
                        if(!p[i]) return EDEADSRCDST;
                        seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
                        phys_addr[i] = umap_remote(p[i], seg_index, vir_addr[i]->offset, bytes);
                        break;
                case BIOS_SEG:
                        if(!p[i]) return EDEADSRCDST;
                        phys_addr[i] = umap_bios(p[i], vir_addr[i]->offset, bytes );
                        break;
                case PHYS_SEG:
                        phys_addr[i] = vir_addr[i]->offset;
                        break;
                default:
                        return(EINVAL);
                }

                /* Check if mapping succeeded. */
                if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG)
                        return(EFAULT);
        }

        /*
        * CHECK ADDRESS RANGE AGAINST VM LIMITS OR PROXY/PROMISCUOS PROCESSES MEMORY LIMITS
        * The caller must be from a VM != VM0
        * and the copy must be LOCAL_SEG->LOCAL_SEG
        */
        if(  (caller_ptr->p_vmid != VM0) &&(type[_SRC_] == LOCAL_SEG &&  type[_DST_] == LOCAL_SEG) ) {
		/*
                * copy between address spaces of processes of the same VM
                */
#ifdef ANULADO					
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_SRC_]));
                MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
                vm_ptr = &vm_hyper[p[_DST_]->p_vmid];
                MHDEBUG(VM_FORMAT, VM_FIELDS(vm_ptr));
                MHDEBUG("SYSTEM: virtual_copy_vm: phys_src=%X  phys_dst=%X bytes=%d\n",
                        phys_addr[_SRC_], phys_addr[_DST_], bytes);
#endif
                if( p[_SRC_]->p_vmid == p[_DST_]->p_vmid) {
                        /* both must be within VM address limits */
                        if( FAULT_VM_LIMITS(p[_SRC_]->p_vmid,phys_addr[_SRC_], bytes))
                                MHERROR("SYSTEM: virtual_copy_vm: src vm limits:%d\n",EFAULT);
                        if( FAULT_VM_LIMITS(p[_DST_]->p_vmid,phys_addr[_DST_], bytes))
                                MHERROR("SYSTEM: virtual_copy_vm: dst vm limits:%d\n",EFAULT);
                        /*
                        * copy from VMx -> VM0(proxy/promiscuous process)
                        */

                } else if (  p[_SRC_]->p_vmid != VM0 && p[_DST_]->p_vmid == VM0) {
#ifdef ANULADO					
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_SRC_]));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
#endif
                        /* check source address limits */
                        if( FAULT_VM_LIMITS(p[_SRC_]->p_vmid,phys_addr[_SRC_], bytes))
                                MHERROR("SYSTEM: virtual_copy_vm: src vm limits:%d\n",EFAULT);

                        /* get the destination descriptor on VMx */
                        p_ptr = proc_addr_vm(p[_SRC_]->p_vmid, proc_nr[_DST_]);

                        if(p[_DST_]->p_misc_flags & PROXY_SERVER) {
                                /* check if the destination proxy server is the proxy of VMx */
                                if(p[_DST_]->p_proxy != p[_SRC_]->p_vmid)
                                        MHERROR("SYSTEM: virtual_copy_vm: not vm proxy:%d\n",EFAULT);

                                /* destination address must be within a PROXY process address space  */
                                if( FAULT_PROC_LIMITS(phys_addr[_DST_],p[_DST_],bytes))
                                        MHERROR("SYSTEM: virtual_copy_vm: dst proc limits:%d\n",EFAULT);
                        } else {
                                if (p_ptr->p_misc_flags & PROMICUOUS_TASK) {
                                        if( FAULT_PROC_LIMITS(phys_addr[_DST_],p[_DST_],bytes))
                                                MHERROR("SYSTEM: virtual_copy_vm: dst proc limits:%d\n",EFAULT);
                                } else {
                                        MHERROR("SYSTEM: virtual_copy_vm: not a proxy:%d\n",EFAULT);
                                }
                        }
                        /*
                        * copy from VM0(proxy/promiscuous process) -> VMx
                        */
                } else if (  p[_SRC_]->p_vmid == VM0 && p[_DST_]->p_vmid != VM0) {
#ifdef ANULADO					
					
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(caller_ptr));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_SRC_]));
                        MHDEBUG(PROC_FORMAT, PROC_FIELDS(p[_DST_]));
#endif

                        /* check destination address limits */
                        if( FAULT_VM_LIMITS(p[_DST_]->p_vmid,phys_addr[_DST_], bytes))
                                MHERROR("SYSTEM: virtual_copy_vm: dst vm limits:%d\n",EFAULT);

                        /* get the source descriptor on VMx */
                        p_ptr = proc_addr_vm(p[_DST_]->p_vmid, proc_nr[_SRC_]);

                        if(p[_SRC_]->p_misc_flags & PROXY_SERVER) {
                                /* check if the destination proxy server is the proxy of VMx */
                                if(p[_SRC_]->p_proxy != p[_DST_]->p_vmid)
                                        MHERROR("SYSTEM: virtual_copy_vm: not vm proxy:%d\n",EFAULT);

                                /* destination address must be within a PROXY process address space  */
                                if(FAULT_PROC_LIMITS(phys_addr[_SRC_],p[_SRC_],bytes))
                                        MHERROR("SYSTEM: virtual_copy_vm: dst proc limits:%d\n",EFAULT);
                        } else {
                                if (p_ptr->p_misc_flags & PROMICUOUS_TASK) {
                                        if( FAULT_PROC_LIMITS(phys_addr[_SRC_],p[_SRC_],bytes))
                                                MHERROR("SYSTEM: virtual_copy_vm: prom proc limits:%d\n",EFAULT);
                                } else {
                                        MHERROR("SYSTEM: virtual_copy_vm: bad dst:%d\n",EFAULT);
                                }
                        }
                        /*
                        * copy from VMx -> VMy !!!!!!!
                        */
                } else {
                        MHERROR("SYSTEM: virtual_copy_vm: can copy on VM0:%d\n",EFAULT);
                }
        }

        /* Now copy bytes between physical addresseses. */
        phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);

        return(OK);
}

#endif /*  MHYPER */
