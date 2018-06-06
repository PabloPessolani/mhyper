#ifndef GLO_H
#define GLO_H

/* Global variables used in the kernel. This file contains the declarations;
 * storage space for the variables is allocated in table.c, because EXTERN is
 * defined as extern unless the _TABLE definition is seen. We rely on the 
 * compiler's default initialization (0) for several global variables. 
 */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/config.h>
#include "config.h"

/* Variables relating to shutting down MINIX. */
EXTERN char kernel_exception;		/* TRUE after system exceptions */
EXTERN char shutdown_started;		/* TRUE after shutdowns / reboots */

EXTERN phys_bytes aout;			/* address of a.out headers */
#ifdef MHYPER

#ifndef INSIDE_KERNEL 
/* Kernel information structures. This groups vital kernel information. */
EXTERN struct kinfo kinfo;		/* kernel information for users */
EXTERN struct machine machine;		/* machine information for users */
EXTERN struct kmessages kmess;  	/* diagnostic messages in kernel */
EXTERN struct randomness krandom;	/* gather kernel random information */
EXTERN struct loadinfo kloadinfo;	/* status of load average */
#endif /*INSIDE_KERNEL */

#else /* MHYPER*/
/* Kernel information structures. This groups vital kernel information. */
EXTERN struct kinfo kinfo;		/* kernel information for users */
EXTERN struct machine machine;		/* machine information for users */
EXTERN struct kmessages kmess;  	/* diagnostic messages in kernel */
EXTERN struct randomness krandom;	/* gather kernel random information */
EXTERN struct loadinfo kloadinfo;	/* status of load average */

#endif /* MHYPER*/

EXTERN irq_hook_t irq_hooks[NR_IRQ_HOOKS];	/* hooks for general use */

/* Process scheduling information and the kernel reentry count. */
EXTERN struct proc *prev_ptr;	/* previously running process */
EXTERN struct proc *proc_ptr;	/* pointer to currently running process */
EXTERN struct proc *next_ptr;	/* next process to run after restart() */
EXTERN struct proc *bill_ptr;	/* process to bill for clock ticks */
EXTERN char k_reenter;		/* kernel reentry count (entry count less 1) */
EXTERN unsigned lost_ticks;	/* clock ticks counted outside clock task */

#if (CHIP == INTEL)

/* Interrupt related variables. */
EXTERN irq_hook_t *irq_handlers[NR_IRQ_VECTORS];/* list of IRQ handlers */
EXTERN int irq_actids[NR_IRQ_VECTORS];		/* IRQ ID bits active */
EXTERN int irq_use;				/* map of all in-use irq's */

/* Miscellaneous. */
EXTERN reg_t mon_ss, mon_sp;		/* boot monitor stack */
EXTERN int mon_return;			/* true if we can return to monitor */
EXTERN int do_serial_debug;
EXTERN int who_e, who_p;		/* message source endpoint and proc */

/* VM */
EXTERN phys_bytes vm_base;
EXTERN phys_bytes vm_size;
EXTERN phys_bytes vm_mem_high;

/* Variables that are initialized elsewhere are just extern here. */
extern struct boot_image image[]; 	/* system image processes */
extern char *t_stack[];			/* task stack space */
extern struct segdesc_s gdt[];		/* global descriptor table */

EXTERN _PROTOTYPE( void (*level0_func), (void) );
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific variables go here. */
#endif

#ifdef MHYPER
EXTERN int	running_vm;		/* running VM */
EXTERN int	IS_active_vm;		/* IS active VM */
EXTERN struct proc *systask_ptr;	/* pointer to SYSTASK in VM0  */
EXTERN struct proc *clock_ptr;		/* pointer to CLOCK   in VM0  */
EXTERN struct proc *hard_ptr;		/* pointer to HARDWARE in VM0  */
EXTERN struct proc *idle_ptr;		/* pointer to IDLE in VM0  */
EXTERN int	debug_switch;		/* debug switch on/off */
EXTERN int	lock_flag;	
EXTERN unsigned int	VM_bitmap;	/* Bitmap of ACTIVE VMs 	*/
EXTERN volatile int	next_vm; 
EXTERN volatile int switch_vm; 

struct vm_metric metric[1024];
#endif /*  MHYPER */

#endif /* GLO_H */
