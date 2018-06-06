#ifndef PROC_H
#define PROC_H

/* Here is the declaration of the process table.  It contains all process
 * data, including registers, flags, scheduling priority, memory map, 
 * accounting, message passing (IPC) information, and so on. 
 *
 * Many assembly code routines reference fields in it.  The offsets to these
 * fields are defined in the assembler include file sconst.h.  When changing
 * struct proc, be sure to change sconst.h to match.
 */
#include <minix/com.h>
#include "protect.h"
#include "const.h"
#include "priv.h"
 
struct proc {
  struct stackframe_s p_reg;	/* process' registers saved in stack frame */

#if (CHIP == INTEL)
  reg_t p_ldt_sel;		/* selector in gdt with ldt base and limit */
  struct segdesc_s p_ldt[2+NR_REMOTE_SEGS]; /* CS, DS and remote segments */
#endif 

#if (CHIP == M68000)
/* M68000 specific registers and FPU details go here. */
#endif 

  proc_nr_t p_nr;		/* number of this process (for fast access) */
  struct priv *p_priv;		/* system privileges structure */
  short p_rts_flags;		/* process is runnable only if zero */
  short p_misc_flags;		/* flags that do suspend the process */

  char p_priority;		/* current scheduling priority */
  char p_max_priority;		/* maximum scheduling priority */
  char p_ticks_left;		/* number of scheduling ticks left */
  char p_quantum_size;		/* quantum size in ticks */

  struct mem_map p_memmap[NR_LOCAL_SEGS];   /* memory map (T, D, S) */

  clock_t p_user_time;		/* user time in ticks */
  clock_t p_sys_time;		/* sys time in ticks */

  struct proc *p_nextready;	/* pointer to next ready process */
#define  p_promiscuous p_nextready
							/* this field is used by process descriptors 			*/
							/* on VMx which represent a promiscuous process on VM0 */
  struct proc *p_caller_q;	/* head of list of procs wishing to send */
  struct proc *p_q_link;	/* link to next proc wishing to send */
  message *p_messbuf;		/* pointer to passed message buffer */
  int p_getfrom_e;		/* from whom does process want to receive? */
  int p_sendto_e;		/* to whom does process want to send? */

  sigset_t p_pending;		/* bit map for pending kernel signals */

  char p_name[P_NAME_LEN];	/* name of the process, including \0 */

  int p_endpoint;		/* endpoint number, generation-aware */

#ifdef MHYPER
  int p_vmid;			/* VM number  */
  int p_proxy;			/* VM number that this proxy represents  */
  int p_recv_vm;	
#endif /*  MHYPER */

#if DEBUG_SCHED_CHECK
  int p_ready, p_found;
#endif
};

/* Bits for the runtime flags. A process is runnable iff p_rts_flags == 0. */
#define SLOT_FREE	0x01	/* process slot is free */
#define NO_MAP		0x02	/* keeps unmapped forked child from running */
#define SENDING		0x04	/* process blocked trying to send */
#define RECEIVING	0x08	/* process blocked trying to receive */
#define SIGNALED	0x10	/* set when new kernel signal arrives */
#define SIG_PENDING	0x20	/* unready while signal being processed */
#define P_STOP		0x40	/* set when process is being traced */
#define NO_PRIV		0x80	/* keep forked system process from running */
#define NO_PRIORITY    0x100	/* process has been stopped */
#define NO_ENDPOINT    0x200	/* process cannot send or receive messages */
#define NOTIFYING      0x400	/* process of p_vmid !=VM0 is blocked trying to notify() a PROMISCOUS TASK (on VM0)*/


/* Misc flags */
#define REPLY_PENDING	0x0001	/* RECEIVE part of SENDREC is pending */
#define MF_VM			0x0008	/* process uses VM */

#ifdef MHYPER
#define VMID_PENDING	0x0002	/* The receiver (a promiscous process on VM0) is waiting that the sender returns its p_vmid */
#define NOT_GETFROM  	0x0004	/* The receiver is waiting a message from any sender except that on p_getfrom_e 			*/

#define PROXY_SERVER	0x0010	/* The process is a VM PROXY SERVER  for descriptor on VM0 */
#define VIRTUAL_TASK	0x0020	/* The process is a VM VIRTUAL TASK   for descriptor on VMx   */
#define PROMICUOUS_TASK	0x0040	/* The process is a VM PROMICUOUS_TASK for descriptor on VMx   */
#define DISABLED_TASK	0x0080	/* The process is DISABLED for descriptor on VMx   */

#define SCHED_LEVELS	0x0F00  /* DONT USE THESE BITS, refer to include/minix/com.h */
#define PARAVIR_TASK 	0x1000	/* Only valid for REAL process types 				*/

#endif /*  MHYPER */



/* Scheduling priorities for p_priority. Values must start at zero (highest
 * priority) and increment.  Priorities of the processes in the boot image 
 * can be set in table.c. IDLE must have a queue for itself, to prevent low 
 * priority user processes to run round-robin with IDLE. 
 */
#define NR_SCHED_QUEUES   16	/* MUST equal minimum priority + 1 */
#define TASK_Q		   0	/* highest, used for kernel tasks */
#define MAX_USER_Q  	   0    /* highest priority for user processes */   
#define USER_Q  	   7    /* default (should correspond to nice 0) */   
#define MIN_USER_Q	  14	/* minimum priority for user processes */
#define IDLE_Q		  15    /* lowest, only IDLE process goes here */


/* Magic process table addresses. */
#ifdef MHYPER

#ifdef INSIDE_KERNEL
#define BEG_PROC_ADDR 	(&vm_hyper[running_vm].vm_proc[0])
#define BEG_USER_ADDR 	(&vm_hyper[running_vm].vm_proc[NR_TASKS])
#define END_PROC_ADDR 	(&vm_hyper[running_vm].vm_proc[NR_TASKS + NR_PROCS])
#define NIL_PROC          ((struct proc *) 0)		
#define NIL_SYS_PROC      ((struct proc *) 1)		
#define cproc_addr(n)     (&(vm_hyper[running_vm].vm_proc + NR_TASKS)[(n)]) 
#define proc_addr(n)      get_proc_addr(n)
#define vect_proc_addr(n) (vm_hyper[running_vm].vm_pproc_addr + NR_TASKS)[(n)]
#else /*INSIDE_KERNEL*/
#define BEG_PROC_ADDR (&proc[0])
#define BEG_USER_ADDR (&proc[NR_TASKS])
#define END_PROC_ADDR (&proc[NR_TASKS + NR_PROCS])
#define NIL_PROC          ((struct proc *) 0)		
#define NIL_SYS_PROC      ((struct proc *) 1)		
#define cproc_addr(n)     (&(proc + NR_TASKS)[(n)])
#define proc_addr(n)      (pproc_addr + NR_TASKS)[(n)]
#endif /*INSIDE_KERNEL*/

#else  /*  MHYPER */
#define BEG_PROC_ADDR (&proc[0])
#define BEG_USER_ADDR (&proc[NR_TASKS])
#define END_PROC_ADDR (&proc[NR_TASKS + NR_PROCS])
#define NIL_PROC          ((struct proc *) 0)		
#define NIL_SYS_PROC      ((struct proc *) 1)		
#define cproc_addr(n)     (&(proc + NR_TASKS)[(n)])
#define proc_addr(n)      (pproc_addr + NR_TASKS)[(n)]
#endif /*  MHYPER */

#define proc_nr(p) 	  ((p)->p_nr)

#define isokprocn(n)      ((unsigned) ((n) + NR_TASKS) < NR_PROCS + NR_TASKS)
#define isemptyn(n)       isemptyp(proc_addr(n)) 
#define isemptyp(p)       ((p)->p_rts_flags == SLOT_FREE)
#define iskernelp(p)	  iskerneln((p)->p_nr)
#define iskerneln(n)	  ((n) < 0 && (n) >= IDLE)
#define isuserp(p)        isusern((p)->p_nr)
#define isusern(n)        ((n) >= 0)

/* The process table and pointers to process table slots. The pointers allow
 * faster access because now a process entry can be found by indexing the
 * pproc_addr array, while accessing an element i requires a multiplication
 * with sizeof(struct proc) to determine the address. 
 */

#ifdef MHYPER

#define PROC_FORMAT "%s:%d vmid=%d nr=%d endp=%d name=%s flags=%X misc=%X getf=%d sent=%d recv_vm=%d\n"
#define PROC_FIELDS(p)  __FILE__ ,__LINE__, p->p_vmid,p->p_nr,p->p_endpoint,p->p_name, p->p_rts_flags , p->p_misc_flags, p->p_getfrom_e , p->p_sendto_e, p->p_recv_vm
#ifdef  INSIDE_KERNEL
struct vm_struct {
	int			vm_vmid;
	int			vm_flags;
	vm_mmap_t		vm_mmap;
    	clock_t 		vm_time;		/* time that the system is in VMx */
	clock_t 		vm_uptime;
	
	phys_bytes		vm_start;		/* physical address of the start of the VM */
	phys_bytes		vm_end;			/* physical address of the end    of the VM */
	
	phys_bytes 		vm_a_out;		/* address of a.out headers */
	struct kinfo 		vm_kinfo;		/* kernel information for users */
	struct machine 		vm_machine;		/* machine information for users */
	struct kmessages 	vm_kmess;  	/* diagnostic messages in kernel */
	struct randomness 	vm_krandom;	/* gather kernel random information */
	struct loadinfo 	vm_kloadinfo;	/* status of load average */

 	struct proc 		vm_proc[NR_TASKS + NR_PROCS];	/* per VM process table */ 
 	struct proc 		*vm_pproc_addr[NR_TASKS + NR_PROCS];
	struct priv 		vm_priv[NR_SYS_PROCS];		/* system properties table */
	struct priv 		*vm_ppriv_addr[NR_SYS_PROCS];	/* direct slot pointers */
	vm_drivers_t		vm_driver[NR_SYS_PROCS];
	vmm_desc_t		vm_VMM;			/* VMM  VM descriptor */	
	char 			vm_boot_params[SECTOR_SIZE + 1]; /* space reserved for boot parameters */
	
	/*Token Bucket*/
	int 			vm_bsize; /* Max Tokens assigned to the VM */
	int 			vm_tokens; /* Tokens assigned to the VM, using for resting */

};
#define VM_FORMAT "%s:%d vmid=%d flags=%X start=%X end=%X\n"
#define VM_FIELDS(v)  __FILE__ ,__LINE__, v->vm_vmid, v->vm_flags , v->vm_start, v->vm_end
#define a_out		vm_hyper[running_vm].vm_a_out
#define	kinfo		vm_hyper[running_vm].vm_kinfo
#define machine	 	vm_hyper[running_vm].vm_machine
#define kmess		vm_hyper[running_vm].vm_kmess
#define krandom		vm_hyper[running_vm].vm_krandom
#define kloadinfo	vm_hyper[running_vm].vm_kloadinfo
#define	vmm_desc(vmid)	vm_hyper[vmid].vm_VMM
#define boot_params	vm_hyper[running_vm].vm_boot_params	
EXTERN struct vm_struct vm_hyper[NR_VMS];
#endif  /*INSIDE_KERNEL */

#ifdef	MINIX_ORIG
struct mnx_proc {
  struct stackframe_s p_reg;	/* process' registers saved in stack frame */

#if (CHIP == INTEL)
  reg_t p_ldt_sel;		/* selector in gdt with ldt base and limit */
  struct segdesc_s p_ldt[2+NR_REMOTE_SEGS]; /* CS, DS and remote segments */
#endif 

#if (CHIP == M68000)
/* M68000 specific registers and FPU details go here. */
#endif 

  proc_nr_t p_nr;		/* number of this process (for fast access) */
  struct priv *p_priv;		/* system privileges structure */
  short p_rts_flags;		/* process is runnable only if zero */
  short p_misc_flags;		/* flags that do suspend the process */

  char p_priority;		/* current scheduling priority */
  char p_max_priority;		/* maximum scheduling priority */
  char p_ticks_left;		/* number of scheduling ticks left */
  char p_quantum_size;		/* quantum size in ticks */

  struct mem_map p_memmap[NR_LOCAL_SEGS];   /* memory map (T, D, S) */

  clock_t p_user_time;		/* user time in ticks */
  clock_t p_sys_time;		/* sys time in ticks */

  struct proc *p_nextready;	/* pointer to next ready process */
  struct proc *p_caller_q;	/* head of list of procs wishing to send */
  struct proc *p_q_link;	/* link to next proc wishing to send */
  message *p_messbuf;		/* pointer to passed message buffer */
  int p_getfrom_e;		/* from whom does process want to receive? */
  int p_sendto_e;		/* to whom does process want to send? */

  sigset_t p_pending;		/* bit map for pending kernel signals */

  char p_name[P_NAME_LEN];	/* name of the process, including \0 */

  int p_endpoint;		/* endpoint number, generation-aware */

#if DEBUG_SCHED_CHECK
  int p_ready, p_found;
#endif
};

#endif /* MINIX_ORIG */

#else  /*  MHYPER */
EXTERN struct proc proc[NR_TASKS + NR_PROCS];	/* process table */
EXTERN struct proc *pproc_addr[NR_TASKS + NR_PROCS];
#endif /*  MHYPER */

EXTERN struct proc *rdy_head[NR_SCHED_QUEUES]; /* ptrs to ready list headers */
EXTERN struct proc *rdy_tail[NR_SCHED_QUEUES]; /* ptrs to ready list tails */

#endif /* PROC_H */
