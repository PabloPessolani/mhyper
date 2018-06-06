/* This file contains a simple exception handler.  Exceptions in user
 * processes are converted to signals. Exceptions in a kernel task cause
 * a panic.
 */

#include "kernel.h"
#include <signal.h>
#include "proc.h"


/*===========================================================================*
 *				exception				     *
 *===========================================================================*/
PUBLIC void exception(vec_nr)
unsigned vec_nr;
{
/* An exception or unexpected interrupt has occurred. */
	
  struct ex_s {
	char *msg;
	int signum;
	int minprocessor;
  };
  static struct ex_s ex_data[] = {
	{ "Divide error", SIGFPE, 86 },
	{ "Debug exception", SIGTRAP, 86 },
	{ "Nonmaskable interrupt", SIGBUS, 86 },
	{ "Breakpoint", SIGEMT, 86 },
	{ "Overflow", SIGFPE, 86 },
	{ "Bounds check", SIGFPE, 186 },
	{ "Invalid opcode", SIGILL, 186 },
	{ "Coprocessor not available", SIGFPE, 186 },
	{ "Double fault", SIGBUS, 286 },
	{ "Copressor segment overrun", SIGSEGV, 286 },
	{ "Invalid TSS", SIGSEGV, 286 },
	{ "Segment not present", SIGSEGV, 286 },
	{ "Stack exception", SIGSEGV, 286 },	/* STACK_FAULT already used */
	{ "General protection", SIGSEGV, 286 },
	{ "Page fault", SIGSEGV, 386 },		/* not close */
	{ NIL_PTR, SIGILL, 0 },			/* probably software trap */
	{ "Coprocessor error", SIGFPE, 386 },
  };
  register struct ex_s *ep;
  struct proc *saved_proc;
#ifdef MHYPER
	int saved_vm, i, j;
	static 	char lcl_canary[CLICK_SIZE] = "HOLA QUE TAL COMO TE VA!"; /* only for testing */
	char *sp_ptr;  /* only for testing */
	phys_bytes proc_phys;
	phys_bytes kernel_phys;
	#endif /*  MHYPER */

  /* Save proc_ptr, because it may be changed by debug statements. */
  saved_proc = proc_ptr;	

  ep = &ex_data[vec_nr];

  if (vec_nr == 2) {		/* spurious NMI on some machines */
	kprintf("got spurious NMI\n");
	return;
  }

  /* If an exception occurs while running a process, the k_reenter variable 
   * will be zero. Exceptions in interrupt handlers or system traps will make 
   * k_reenter larger than zero.
   */
  if (k_reenter == 0 && !iskernelp(saved_proc)) {
#ifdef MHYPER
	lock(0, "exception");
	saved_vm = running_vm;
	MHDEBUG("exception proc=%d name=%s vmid=%d running_vm=%d vec_nr=%d sp=%X %s\n",
		saved_proc->p_nr, saved_proc->p_name, saved_proc->p_vmid, running_vm, vec_nr, saved_proc->p_reg.sp, ep->msg );
	
	if(saved_proc->p_vmid != VM0) {
		MHDEBUG("DATA proc=%d name=%s vir=%X phys=%X len=%X\n",saved_proc->p_nr, saved_proc->p_name,
			saved_proc->p_memmap[D].mem_vir,
			saved_proc->p_memmap[D].mem_phys, 
			saved_proc->p_memmap[D].mem_len);
			kernel_phys = VIR2PHYS(&lcl_canary);
		for( i = (saved_proc->p_reg.sp >> CLICK_SHIFT)  ;  i < saved_proc->p_memmap[D].mem_len; i++) {	
			proc_phys = umap_local(saved_proc, D, i << CLICK_SHIFT, CLICK_SIZE);
			phys_copy(proc_phys, kernel_phys, (phys_bytes) CLICK_SIZE); 
			for( j = 0 ;  j < CLICK_SIZE-8; j+=8) {
				kprintf("[%X] %X %X %X %X %X %X %X %X %c%c%c%c%c%c%c%c\n", (i << CLICK_SHIFT)+j,
					lcl_canary[j+0],
					lcl_canary[j+1],
					lcl_canary[j+2],
					lcl_canary[j+3],
					lcl_canary[j+4],
					lcl_canary[j+5],
					lcl_canary[j+6],
					lcl_canary[j+7],
					lcl_canary[j+0],
					lcl_canary[j+1],
					lcl_canary[j+2],
					lcl_canary[j+3],
					lcl_canary[j+4],
					lcl_canary[j+5],
					lcl_canary[j+6],
					lcl_canary[j+7]
					);
			}
		}
	} 
	
	running_vm= saved_proc->p_vmid;
	cause_sig(proc_nr(saved_proc), ep->signum);
	running_vm = saved_vm;
	unlock(0);
#else /* MHYPER */
	cause_sig(proc_nr(saved_proc), ep->signum);
#endif /*  MHYPER */
	return;
  }

  /* Exception in system code. This is not supposed to happen. */
  if (ep->msg == NIL_PTR || machine.processor < ep->minprocessor)
	kprintf("\nIntel-reserved exception %d\n", vec_nr);
  else
	kprintf("\n%s\n", ep->msg);
  kprintf("k_reenter = %d ", k_reenter);
#ifdef MHYPER
  kprintf("running_vm=%d  lock_flag=%d\n", running_vm, lock_flag);
#endif /*  MHYPER */

  kprintf("process %d (%s), ", proc_nr(saved_proc), saved_proc->p_name);
  kprintf("pc = %u:0x%x %X", (unsigned) saved_proc->p_reg.cs,
  (unsigned) saved_proc->p_reg.pc, CODE2PHYS((saved_proc->p_reg.pc)));

#ifdef MHYPER
  kprintf("ENDING VM%d\n", running_vm);
  panic("exception in a MINIX task/server", NO_NUM);
#else /*  MHYPER */
  panic("exception in a kernel task", NO_NUM);
#endif /*  MHYPER */
}

