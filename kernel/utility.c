/* This file contains a collection of miscellaneous procedures:
 *   panic:	    abort MINIX due to a fatal error
 */

#include "kernel.h"
#include "proc.h"

#include <unistd.h>
#ifdef MHYPER
#include <limits.h>
#endif /*  MHYPER */

/*===========================================================================*
 *				panic                                        *
 *===========================================================================*/
PUBLIC void panic(mess,nr)
_CONST char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  static int panicking = 0;
  if (panicking ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
	kprintf("\nKernel panic: %s", mess);
	if (nr != NO_NUM) kprintf(" %d", nr);
	kprintf("\n",NO_NUM);
  }

  /* Abort MINIX. */
  prepare_shutdown(RBT_PANIC);
}

#ifdef MHYPER
/*===========================================================================*
 *				proc_addr_vm                             *
 *===========================================================================*/
PUBLIC struct proc *proc_addr_vm(vmid, proc_nr)
int vmid;
int proc_nr;
{
	struct proc *rp;
	
	if( !isokprocn(proc_nr) ||
		( vmid < VM0 || vmid >= NR_VMS)) {
		kprintf("proc_addr_vm: ERROR vmid=%d proc_nr=%d proc_ptr->p_name=%s\n", 
			vmid, proc_nr, proc_ptr->p_name);
		return(NULL);
	}
	
 	if( proc_nr == SYSTEM ) {
		rp = systask_ptr;
	} else if (proc_nr == CLOCK ) {
		rp = clock_ptr;
	} else if (proc_nr == HARDWARE ) {
		rp = hard_ptr;
	} else if (proc_nr == IDLE ) {
		rp = idle_ptr;
	} else {
		rp = &vm_hyper[vmid].vm_proc[proc_nr+NR_TASKS];
	}
	return(rp);
}


/*===========================================================================*
 *				get_proc_addr                                        *
 *===========================================================================*/
PUBLIC struct proc *get_proc_addr(proc_nr)
int proc_nr;
{
	struct proc *rp;
	
	if( !isokprocn(proc_nr)) {
		kprintf("get_proc_addr: ERROR proc_nr=%d  proc_ptr->p_name=%s running_vm=%d\n",
			proc_nr, proc_ptr->p_name, running_vm);
		return(NULL);
	}
	if( running_vm < 0 || running_vm >= (NR_VMS)) {
		kprintf("get_proc_addr: running_vm=%d\n", running_vm);
		return(NULL);
	}
	
 	if( proc_nr == SYSTEM ) {
		rp = systask_ptr;
	} else if (proc_nr == CLOCK ) {
		rp = clock_ptr;
	} else if (proc_nr == HARDWARE ) {
		rp = hard_ptr;
	} else if (proc_nr == IDLE ) {
		rp = idle_ptr;
	} else {
		rp = vect_proc_addr(proc_nr);
	}
	return(rp);
}

/*===========================================================================*
 *				next_rvm				                                        	*
 * This routine is called on every kernel exit 							* 
 *===========================================================================*/
PUBLIC void next_rvm(void)
{
	running_vm = next_ptr->p_vmid;
}

/*===========================================================================*
 *				proc_rvm				                                        *
 * This routine is called on every kernel entry							* 
 *===========================================================================*/
PUBLIC void proc_rvm(void)
{
	running_vm = proc_ptr->p_vmid;
}

/*===========================================================================*
 *				ul2a                                        *
* Transform a long number to ascii at base b, (b >= 8). *
 *===========================================================================*/
#define arraysize(a)		(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)		((a) + arraysize(a))

char *ul2a(u32_t n, unsigned b)
{
	static char num[(CHAR_BIT * sizeof(n) + 2) / 3 + 1];
	char *a= arraylimit(num) - 1;
	static char hex[16] = "0123456789ABCDEF";

	do *--a = hex[(int) (n % b)]; while ((n/= b) > 0);
	return a;
}

#endif /*  MHYPER */

