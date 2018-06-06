/* The kernel call that is implemented in this file:
 *   m_type:	SYS_KILLVM
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process to signal/ pending		
 *     m2_i2:	SIG_NUMBER	# signal number to send to process
 *     m2_i3:	SIG_VMID	# VMID of the process to signal 
  */

#include "../system.h"
#include <signal.h>
#include <sys/sigcontext.h>

#if USE_KILLVM

/*===========================================================================*
 *			          do_killvm				     *
 *===========================================================================*/
PUBLIC int do_killvm(m_ptr)
message *m_ptr;			/* pointer to request message */
{
	proc_nr_t proc_nr, proc_nr_e;
	int sig_nr, vmid, saved_vm;
    struct proc *caller_ptr;

	
	proc_nr_e= m_ptr->SIG_ENDPT;
	sig_nr = m_ptr->SIG_NUMBER;
	vmid = m_ptr->SIG_VMID;

	if(vmid < 0 || vmid > NR_VMS)  return(EINVAL);
	if( running_vm != VM0 ) return(EPERM);
	if (sig_nr > _NSIG) return(EINVAL);
	
	/* Is the VM Loaded and Running			*/
	if ( vmid != VM0 ) {
#ifdef IPC_DBG
		MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
#endif /* IPC_DBG */ 		
		if( vm_hyper[vmid].vm_flags != (VM_LOADED | VM_RUNNING) ) {
			return(EVMSTATUS);
		}
	}
	
	caller_ptr = proc_addr(who_p);
	
	/* Was the VM recognized by the promiscuous proc?*/
	if( !(priv(caller_ptr)->s_vm_bitmap & (1 << vmid))) {
		return(EBADVMID);
	}
	
	if (proc_nr_e == SELF) /* SUICIDE PROCESS!!!! */
		proc_nr_e= m_ptr->m_source;
	
	if(proc_nr_e	== TTY_PROC_NR)
		MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	
	
	saved_vm = running_vm;
	running_vm = vmid;
	if (!isokendpt(proc_nr_e, &proc_nr)){
		running_vm = saved_vm;
		return(EINVAL);
	}

	if (iskerneln(proc_nr)) {
		running_vm = saved_vm;
		return(EPERM);
	}
	/* Set pending signal to be processed by the PM. */

	cause_sig(proc_nr, sig_nr);
	if (sig_nr == SIGKILL) {
		if(proc_nr	== TTY_PROC_NR)
			MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
		clear_endpoint(proc_addr(proc_nr));
	}
	running_vm = saved_vm;
	return(OK);
}

#endif /* USE_KILL */

